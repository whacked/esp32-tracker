(ns codegen
  )

(babashka.deps/add-deps '{:deps {metosin/malli {:mvn/version "0.17.0"}}})
(require '[babashka.pods :as pods])
(require '[malli.core :as m])
(require '[clojure.pprint :as pprint])
(require '[clojure.java.io :as io])

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
               [:records [:sequential any?]]
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


(defn emit-python-method [{:keys [name args doc]}]
  (let [pyargs (clojure.string/join ", " (cons "self" (map #(str (:name %) "") args)))
        docstring (str "\"\"\"" doc
                       (when (seq args)
                         (str "\n\nArgs:\n"
                              (apply str
                                     (for [{:keys [name type doc]} args]
                                       (str "    " name " (" (python-type type) "): " doc "\n")))))
                       "\"\"\"")]
    (str "    def " name "(" pyargs "):\n"
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

(defn emit-python-file [commands out-path]
  (with-open [w (io/writer out-path)]
    (.write w "# AUTO-GENERATED FILE. DO NOT EDIT.\n\n")
    (.write w "from typing import Any\n\n")
    (.write w (emit-python-enum commands))
    (.write w "class BaseCommandHandler:\n")
    (doseq [cmd commands]
      (.write w (emit-python-method cmd)))
    (.write w "\nCOMMAND_SPECS = {\n")
    (doseq [cmd commands]
      (.write w (emit-python-spec-entry cmd)))
    (.write w "}\n")))

(when-let [output-file-path (last *command-line-args*)]

  (emit-python-file commands output-file-path)
  (println (str "wrote to " output-file-path))

  (System/exit 0))
