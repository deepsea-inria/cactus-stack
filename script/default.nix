{ pkgs   ? import <nixpkgs> {},
  stdenv ? pkgs.stdenv,
  fetchurl
}:

stdenv.mkDerivation rec {
  name = "cactus-stack-${version}";
  version = "v1.0";

  src = fetchurl {
    url = "https://github.com/deepsea-inria/cactus-stack/archive/${version}.tar.gz";
    sha256 = "0ab9wc0h94h69w7grzlpiq2vwz2823n1hq4cz1pccb6vpfi0771d";
  };

  installPhase = ''
    mkdir -p $out/include/
    cp include/*.hpp $out/include/
    mkdir -p $out/test
    cp test/* $out/test/
  '';

  meta = {
    description = "A small, self-contained header file that implements the Deepsea command-line conventions.";
    license = "MIT";
    homepage = http://deepsea.inria.fr/;
  };
}