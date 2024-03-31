{
  wrapCCWith,
  include-what-you-use,
  llvmPackages_14,
}:
# see: https://github.com/NixOS/nixpkgs/compare/master...Et7f3:nixpkgs:fix-include-what-you-use
wrapCCWith rec {
  cc = include-what-you-use;
  extraBuildCommands = ''
    wrap include-what-you-use $wrapper $ccPath/include-what-you-use
    substituteInPlace "$out/bin/include-what-you-use" --replace 'dontLink=0' 'dontLink=1'
    substituteInPlace "$out/bin/include-what-you-use" --replace ' && isCxx=1 || isCxx=0' '&&  true; isCxx=1'

    rsrc="$out/resource-root"
    mkdir "$rsrc"
    ln -s "${llvmPackages_14.clang}/resource-root/include" "$rsrc"
    echo "-resource-dir=$rsrc" >> $out/nix-support/cc-cflags
  '';
  libcxx = llvmPackages_14.libcxx;
}
