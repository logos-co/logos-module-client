# Builds everything - shared build derivation
{ pkgs, common, src }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-build";
  version = common.version;

  inherit src;
  inherit (common) nativeBuildInputs buildInputs cmakeFlags meta;
}
