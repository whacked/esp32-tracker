(ns codegen
  )

(babashka.deps/add-deps '{:deps {metosin/malli {:mvn/version "0.17.0"}}})
(require '[babashka.pods :as pods])
(require '[malli.core :as m])
(require '[clojure.pprint :as pprint])
(require '[clojure.java.io :as io])

(def RecordType
  [:enum :measurement :sip :refill])

(def EpochTime (fn [x] (int? x)))

(def Record
  [:map
   [:start_time EpochTime]
   [:end_time EpochTime]
   [:grams float?]
   [:type RecordType]])

(def CommandArg
  [:map
   [:name [:or keyword? string?]]
   [:type fn?]
   [:doc string?]
   [:default {:optional true} any?]])

(def Command
  [:map
   [:name string?]
   [:args [:sequential CommandArg]]
   [:response [:or nil? fn? [:vector any?]]]
   [:doc string?]])

(def CommandsSchema
  [:sequential Command])

(def commands
  [{:name "getVersion"
    :args []
    :response string?
    :doc "Get firmware version (string)"}

   {:name "setTime"
    :args [{:name "epoch" :type int? :doc "Unix epoch seconds"}]
    :response [:map
               [:status string?]
               [:offset int?]
               [:time string?]]
    :doc "Set device time to given epoch"}

   {:name "clearBuffer"
    :args []
    :response nil
    :doc "Clear the data buffer"}

   {:name "readBuffer"
    :args [{:name "offset" :type int? :default 0 :doc "Start index"}
           {:name "length" :type int? :default 20 :doc "Number of records"}]
    :response [:map
               [:records [:sequential Record]]
               [:length int?]]
    :doc "Read paginated buffer"}

   {:name "startLogging"
    :args []
    :response nil
    :doc "Enable logging"}

   {:name "stopLogging"
    :args []
    :response nil
    :doc "Disable logging"}

   {:name "getNow"
    :args []
    :response [:map
               [:epoch int?]
               [:local string?]]
    :doc "Get current device time"}

   {:name "getStatus"
    :args []
    :response [:map
               [:logging boolean?]
               [:bufferSize int?]
               [:rateHz int?]]
    :doc "Get device status"}

   {:name "setSamplingRate"
    :args [{:name "rate" :type int? :doc "Sampling rate in Hz"}]
    :response nil
    :doc "Set sampling rate"}

   {:name "calibrate"
    :args [{:name "low" :type int? :doc "No-load reading"}
           {:name "high" :type int? :doc "Loaded reading"}
           {:name "weight" :type int? :doc "Actual weight in grams"}]
    :response nil
    :doc "Calibrate the scale"}

   {:name "reset"
    :args []
    :response nil
    :doc "Reset the device"}

   {:name "setLogLevel"
    :args [{:name "printer" :type string? :doc "Printer name (raw/event/status)"}
           {:name "level" :type int? :doc "Log level"}]
    :response [:map
               [:status string?]
               [:printer string?]
               [:level int?]]
    :doc "Set log level for a printer"}

   {:name "dropRecords"
    :args [{:name "offset" :type int? :doc "Start index"}
           {:name "length" :type int? :doc "Number of records"}]
    :response [:map
               [:status string?]
               [:offset int?]
               [:length int?]]
    :doc "Drop records from buffer"}

   {:name "unknown"
    :args []
    :response string?
    :doc "Unknown command (error)"}
  ])

(comment
  (println "Validating good commands:")

  (println (m/validate CommandsSchema commands))

  (when (not (m/validate CommandsSchema commands))
    (println "\nExplain failure:")
    (clojure.pprint/pprint (m/explain CommandsSchema commands)))

  (def bad-commands
    (-> commands
        ;; Remove :name from the first command
        (update 0 dissoc :name)
        ;; Change :args to a string in the second command
        (update 1 assoc :args "not-a-vector")
        ;; Remove :doc from the third command
        (update 2 dissoc :doc)
        ;; Change :response to a number in the fourth command
        (update 3 assoc :response 42)))

  (println "\nValidating bad commands:")
  (println (m/validate CommandsSchema bad-commands))

  (when (not (m/validate CommandsSchema bad-commands))
    (println "\nExplain failure for bad commands:")
    (clojure.pprint/pprint (m/explain CommandsSchema bad-commands))))


(defn upper-snake-case [s]
  (-> s
      (clojure.string/replace #"([a-z])([A-Z])" "$1_$2")
      (clojure.string/replace #"-" "_")
      clojure.string/upper-case))

(def python-type-map
  {string? "str"
   int? "int"
   float? "float"
   boolean? "bool"
   'string? "str"
   'int? "int"
   'float? "float"
   'boolean? "bool"})

(defn python-type [clj-type]
  (get python-type-map clj-type "Any"))

(defn capitalize-first-char [s]
  (str (clojure.string/upper-case (subs s 0 1))
       (subs s 1)))

(defn class-name-for-schema [command-name suffix]
  (str (capitalize-first-char command-name)
       suffix))

(defn collect-typed-dicts-in-schema [schema class-name acc]
  (cond
    (and (vector? schema) (= (first schema) :map))
    (do
      (let [fields (rest schema)
            acc' (reduce
                  (fn [a [k v]]
                    (collect-typed-dicts-in-schema
                     v
                     (str class-name (capitalize-first-char (name k))) a))
                  acc
                  fields)]
        (if (contains? acc' schema)
          acc'
          (assoc acc' schema class-name))))
    (and (vector? schema) (= (first schema) :sequential))
    (collect-typed-dicts-in-schema (second schema) (str class-name "Item") acc)
    :else acc))

(defn collect-typed-dicts
  "Recursively collects all map schemas in responses, returns a map of {schema -> class-name}."
  ([commands] (collect-typed-dicts commands {}))
  ([commands acc]
   (reduce
    (fn [acc {:keys [name response]}]
      (collect-typed-dicts-in-schema response (class-name-for-schema name "Response") acc))
    acc
    commands)))

(defn python-return-type [response class-map]
  (cond
    (nil? response) "None"
    (or (= response 'string?) (= response string?)) "str"
    (or (= response 'int?) (= response int?)) "int"
    (or (= response 'boolean?) (= response boolean?)) "bool"
    (or (= response 'float?) (= response float?)) "float"
    (and (vector? response) (= (first response) :map)) (class-map response)
    (and (vector? response) (= (first response) :sequential))
    (str "List[" (python-return-type (second response) class-map) "]")
    :else "Any"))

(defn emit-python-method [{:keys [name args doc response]} class-map]
  (let [pyargs (clojure.string/join ", " (cons "self" (map #(str (:name %) "") args)))
        return-type (python-return-type response class-map)
        docstring (str "\"\"\"" doc
                       (when (seq args)
                         (str "\n\nArgs:\n"
                              (apply str
                                     (for [{:keys [name type doc]} args]
                                       (str "    " name " (" (python-type type) "): " doc "\n")))))
                       (when (not= return-type "None")
                         (str "\n\nReturns:\n    " return-type))
                       "\"\"\"")]
    (str "    async def " name "(" pyargs ")"
         " -> " return-type ":\n"
         "        " docstring "\n"
         "        pass\n\n")))

(defn emit-python-spec-entry [{:keys [name args doc]}]
  (str "    \"" name "\": {\n"
       "        \"args\": [\n"
       (apply str
              (for [{:keys [name type doc]} args]
                (str "            {\"name\": \"" name "\", \"type\": " (python-type type) ", \"doc\": \"" doc "\"},\n")))
       "        ],\n"
       "        \"doc\": \"" doc "\"\n"
       "    },\n"))

(defn emit-python-enum [commands]
  (str "from enum import Enum\n\n"
       "class Command(Enum):\n"
       (apply str
              (for [{:keys [name]} commands]
                (str "    " (upper-snake-case name) " = \"" name "\"\n")))
       "\n"))

(defn emit-typed-dict [class-name schema class-map]
  (let [fields (rest schema)]
    (str "class " class-name "(TypedDict):\n"
         (apply str
                (for [[k v] fields]
                  (str "    " (name k) ": "
                       (cond
                         (and (vector? v) (= (first v) :map))
                         (class-map v)
                         (and (vector? v) (= (first v) :sequential))
                         (str "List[" (if (and (vector? (second v)) (= (first (second v)) :map))
                                        (class-map (second v))
                                        (python-type (second v))) "]")
                         :else (python-type v))
                       "\n")))
         (when (empty? fields) "    pass\n")
         "\n")))

(defn emit-python-file [commands out-path]
  (let [class-map (collect-typed-dicts commands)
        typed-dict-defs (->> class-map
                             (map (fn [[schema class-name]]
                                    (emit-typed-dict class-name schema class-map)))
                             (clojure.string/join "\n"))]
    (with-open [w (io/writer out-path)]
      (.write w "# AUTO-GENERATED FILE. DO NOT EDIT.\n\n")
      (.write w "from typing import Any, List, TypedDict\n\n")
      (.write w (emit-python-enum commands))
      (.write w typed-dict-defs)
      (.write w "class BaseCommandHandler:\n")
      (doseq [cmd commands]
        (.write w (emit-python-method cmd class-map)))
      (.write w "\nCOMMAND_SPECS = {\n")
      (doseq [cmd commands]
        (.write w (emit-python-spec-entry cmd)))
      (.write w "}\n"))))

(def cpp-type-map
  {string? "std::string"
   int? "int"
   float? "float"
   boolean? "bool"
   'string? "std::string"
   'int? "int"
   'float? "float"
   'boolean? "bool"
   RecordType "RecordType"
   EpochTime "time_t"})

(defn cpp-type [clj-type]
  (cond
    (and (vector? clj-type) (= (first clj-type) :enum))
    "RecordType" ; or dynamically (name of the enum)
    :else (get cpp-type-map clj-type)))

(defn cpp-struct-name [command-name suffix]
  (str (capitalize-first-char command-name) suffix))

(defn emit-cpp-struct [class-name schema class-map]
  (let [fields (rest schema)]
    (str "struct " class-name " {\n"
         (apply str
                (for [[k v] fields]
                  (str "    "
                       (cond
                         (and (vector? v) (= (first v) :map))
                         (class-map v)
                         (and (vector? v) (= (first v) :sequential))
                         (str "std::vector<" (if (and (vector? (second v)) (= (first (second v)) :map))
                                               (class-map (second v))
                                               (cpp-type (second v))) ">")
                         :else (cpp-type v))
                       " " (name k) ";\n")))
         "};\n\n")))

(defn emit-cpp-enum [commands]
  (str "enum class Command {\n"
       (apply str
              (for [{:keys [name]} commands]
                (str "    " (capitalize-first-char name) ",\n")))
       "};\n\n"))

(defn emit-cpp-constants [commands]
  (apply str
         (for [{:keys [name]} commands]
           (str "constexpr const char* CMD_"
                (clojure.string/upper-case (clojure.string/replace name #"-" "_"))
                " = \"" name "\";\n")))
  )

(defn collect-cpp-structs-in-schema [schema class-name acc]
  (cond
    (and (vector? schema) (= (first schema) :map))
    (let [fields (rest schema)
          acc' (reduce
                (fn [a [k v]]
                  (collect-cpp-structs-in-schema
                   v
                   (str class-name (capitalize-first-char (name k))) a))
                acc
                fields)]
      (if (contains? acc' schema)
        acc'
        (assoc acc' schema class-name)))
    (and (vector? schema) (= (first schema) :sequential))
    (collect-cpp-structs-in-schema (second schema) (str class-name "Item") acc)
    :else acc))

(defn collect-cpp-structs
  ([commands] (collect-cpp-structs commands {}))
  ([commands acc]
   (reduce
    (fn [acc {:keys [name response]}]
      (collect-cpp-structs-in-schema response (cpp-struct-name name "Response") acc))
    acc
    commands)))

(def cpp-json-escape-fn
  (slurp "src/util.cpp"))

(defn emit-cpp-json-array [field-name item-type]
  (str
    "\"[\" +\n"
    "        [&](){\n"
    "            std::string arr;\n"
    "            for (size_t i = 0; i < r." field-name ".size(); ++i) {\n"
    "                if (i > 0) arr += \",\";\n"
    "                arr += " item-type "ToJson(r." field-name "[i]);\n"
    "            }\n"
    "            return arr;\n"
    "        }() + \"]\""))

;; Update this function:
(defn emit-cpp-tojson [class-name schema class-map]
  (let [fields (rest schema)]
    (str "inline std::string " class-name "ToJson(const " class-name "& r) {\n"
         "    return std::string(\"{\") +\n"
         (clojure.string/join " +\n"
           (map-indexed
            (fn [i [k v]]
              (let [comma (if (zero? i) "" ",")
                    field (name k)
                    key (str "\\\"" field "\\\":")
                    value-expr
                    (cond
                      (or (= v 'string?) (= v string?))
                      (str "json_escape(r." field ")")

                      (or (= v 'int?) (= v int?) (= v 'float?) (= v float?))
                      (str "std::to_string(r." field ")")

                      (or (= v 'boolean?) (= v boolean?))
                      (str "(r." field " ? \"true\" : \"false\")")

                      (and (vector? v) (= (first v) :map))
                      (str (class-map v) "ToJson(r." field ")")

                      (and (vector? v) (= (first v) :sequential))
                      (let [item-type (if (and (vector? (second v)) (= (first (second v)) :map))
                                        (class-map (second v))
                                        (cpp-type (second v)))]
                        (emit-cpp-json-array field item-type))

                      (= v EpochTime)
                      (str "std::to_string(r." field ")")

                      (= v RecordType)
                      (str "\"\\\"\" + RecordTypeToString(r." field ") + \"\\\"\"")

                      :else "\"null\"")]
                (str "        \"" comma key "\" + " value-expr)))
            fields))
         " + \"}\";\n"
         "}\n\n")))

(defn emit-cpp-enum-from-malli [enum-name malli-def]
  (let [[_ & values] malli-def]
    (str "enum " enum-name " {\n"
         (apply str
                (map (fn [v]
                       (str "    " (clojure.string/upper-case (name v)) ",\n"))
                     values))
         "};\n\n")))

(defn emit-cpp-struct-from-malli [struct-name malli-def type-map]
  (let [[_ & fields] malli-def]
    (str "struct " struct-name " {\n"
         (apply str
                (map (fn [[k v]]
                       (str "    " (or (type-map v)
                                       (if (and (vector? v) (= (first v) :enum))
                                         (name k) ; fallback for unknown type
                                         "/*UnknownType*/"))
                            " " (name k) ";\n"))
                     fields))
         "};\n\n")))

(defn emit-cpp-enum-to-string-fn [enum-name malli-def]
  (let [[_ & values] malli-def]
    (str "inline const char* " enum-name "ToString(" enum-name " t) {\n"
         "    switch (t) {\n"
         (apply str
                (map (fn [v]
                       (str "        case " (clojure.string/upper-case (name v))
                            ": return \"" (name v) "\";\n"))
                     values))
         "        default: return \"unknown\";\n"
         "    }\n"
         "}\n\n")))

(defn emit-cpp-header-file [commands out-path]
  (let [class-map (collect-cpp-structs commands)
        struct-and-tojson-defs (->> class-map
                                    (map (fn [[schema class-name]]
                                           (str (emit-cpp-struct class-name schema class-map)
                                                (emit-cpp-tojson class-name schema class-map)))
                                    )
                                    (clojure.string/join "\n"))]
    (with-open [w (io/writer out-path)]
      (.write w "// AUTO-GENERATED FILE. DO NOT EDIT.\n\n")
      (.write w "#pragma once\n")
      (.write w "#include <string>\n#include <vector>\n#include <time.h>\n\n")
      (.write w (emit-cpp-enum-from-malli "RecordType" RecordType))
      (.write w (emit-cpp-enum-to-string-fn "RecordType" RecordType))
      (.write w (emit-cpp-struct-from-malli "Record" Record cpp-type-map))
      (.write w (emit-cpp-tojson "Record" Record cpp-type-map))
      (.write w cpp-json-escape-fn)
      (.write w (emit-cpp-enum commands))
      (.write w (emit-cpp-constants commands))
      (.write w "\n")
      (.write w struct-and-tojson-defs))))

(let [output-file-path (last *command-line-args*)]
  (cond
    (clojure.string/ends-with? output-file-path ".py")
    (do
      (emit-python-file commands output-file-path)
      (println (str "wrote to " output-file-path))
      (System/exit 0))

    (clojure.string/ends-with? output-file-path ".h")
    (do
      (emit-cpp-header-file commands output-file-path)
      (println (str "wrote to " output-file-path))
      (System/exit 0))

    :else
    (do
      (println "Unknown file extension for codegen output!")
      (System/exit 1))))
