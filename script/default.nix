{ pkgs   ? import <nixpkgs> {},
  stdenv ? pkgs.stdenv,
  cactusStackSrc ? ../.
}:

stdenv.mkDerivation rec {
  name = "cactus-stack-${version}";
  version = "v1.0";

  src = cactusStackSrc;

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
