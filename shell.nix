{ pkgs ? import <nixpkgs> {} }:

let
  # provides "echo-shortcuts"
  nix_shortcuts = import (pkgs.fetchurl {
    url = "https://raw.githubusercontent.com/whacked/setup/refs/heads/master/bash/nix_shortcuts.nix.sh";
    hash = "sha256-jCOOVA9rILbvYSKI6rAR/OdP1gv6jDFiiTG9FgJEnvg=";
  }) { inherit pkgs; };

  python = pkgs.python3Full;
  pythonPackages = pkgs.python3Packages;

in pkgs.mkShell {
  buildInputs = [
    # pkgs.websocat
  ] ++ [
    # (pkgs.poetry.override ({ python3 = python; }))
    python
    pythonPackages.ipython
    pythonPackages.async-timeout
    pythonPackages.numpy
    pythonPackages.matplotlib
    pythonPackages.pandas
    pythonPackages.pyaml
    pythonPackages.psutil
    pkgs.mdsh
    pkgs.pkg-config
    pkgs.gtest              # C++ Google Test library
    pkgs.rlwrap
    pkgs.babashka
  ] ++ (with pkgs; [
    gnumake
    gcc
  ]) ++
  nix_shortcuts.buildInputs
  ;  # join lists with ++

  nativeBuildInputs = [
  ];

  name = "esp32-water-tracker";

  shellHook = nix_shortcuts.shellHook + ''
    _setup-venv() {
      pip install bleak
    }
    ensure-venv _setup-venv

    alias bt="rlwrap python bttest.py"
  '' + ''
    echo-shortcuts ${__curPos.file}
  '';  # join strings with +
}
