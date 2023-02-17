{
  description = "Vere build devshell";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
    parts = {
      url = "github:hercules-ci/flake-parts";
      inputs.nixpkgs-lib.follows = "nixpkgs";
    };
  };

  outputs = inputs@{ self, nixpkgs, parts }: parts.lib.mkFlake { inherit inputs; } (let
    # map systems to the musl-cross-make target names; only
    # x86_64-linux is tested though
    toolchainTargets = {
      "x86_64-linux" = "x86_64-linux-musl";
      "aarch64-linux" = "aarch64-linux-musl";
    };
    # map dep urls to hashes
    toolchainDeps = {
      "https://ftpmirror.gnu.org/gnu/gcc/gcc-9.4.0/gcc-9.4.0.tar.xz" = "13l3p6g2krilaawbapmn9zmmrh3zdwc36mfr3msxfy038hps6pf9";
      "https://ftpmirror.gnu.org/gnu/binutils/binutils-2.33.1.tar.xz" = "1grcf8jaw3i0bk6f9xfzxw3qfgmn6fgkr108isdkbh1y3hnzqrmb";
      "https://musl.libc.org/releases/musl-1.2.3.tar.gz" = "196lrzw0qy5axiz9p5ay50q2mls8hbfckr4rw0klc7jjc9h0nnvx";
      "https://ftpmirror.gnu.org/gnu/gmp/gmp-6.1.2.tar.bz2" = "1clg7pbpk6qwxj5b2mw0pghzawp2qlm3jf9gdd8i6fl6yh2bnxaj";
      "https://ftpmirror.gnu.org/gnu/mpc/mpc-1.1.0.tar.gz" = "0biwnhjm3rx3hc0rfpvyniky4lpzsvdcwhmcn7f0h4iw2hwcb1b9";
      "https://ftpmirror.gnu.org/gnu/mpfr/mpfr-4.0.2.tar.bz2" = "1k1s4p56272bggvyrxfn3zdycr4wy7h5ipac70cr03lys013ypn0";
      "http://ftp.barfooze.de/pub/sabotage/tarballs//linux-headers-4.19.88-1.tar.xz" = "04r8k4ckqbklx9sfm07cr7vfw5ra4cic0rzanm9dfh0crxncfnwr";
      "http://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.sub;hb=3d5db9ebe860" = "75d5d255a2a273b6e651f82eecfabf6cbcd8eaeae70e86b417384c8f4a58d8d3";
    };
  in {
    systems = builtins.attrNames toolchainTargets;

    flake = {};

    perSystem = { pkgs, system, ... }: let
      target = toolchainTargets.${system};

      # in order to make the toolchain derivation pure, spoof `wget`
      # to use FODs for the dependencies that would otherwise be
      # downloaded
      pseudoWget = let
        c = pkgs.coreutils;
      in pkgs.writeScriptBin "wget" ''
        #!${pkgs.stdenv.shell}

        declare -A targets
        ${builtins.concatStringsSep "\n"
          (builtins.map (url:
            "targets[${nixpkgs.lib.escapeShellArg url}]=" + (nixpkgs.lib.escapeShellArg (pkgs.fetchurl {
              inherit url;
              sha256 = builtins.getAttr url toolchainDeps;
            })))
            (builtins.attrNames toolchainDeps))}

        # -c -O target url
        target="$3"
        url="$4"
        preFetched=${"$" + "{targets[$url]}"}
        if [[ -z "$preFetched" ]]; then
          ${c}/bin/echo 1>&2 "$target ($url) is new or changed, nix-prefetch-url it and add or update the url and hash in toolchainDeps in flake.nix"
          exit 1
        fi
        ${c}/bin/cp --reflink=auto "$preFetched" "$target"
      '';

      toolchain = let
        # keep in sync with `_musl_cross_make_version` in
        # bazel/toolchain/BUILD.bazel
        version = "fe915821b652a7fa37b34a596f47d8e20bc72338";

        # `-g -O2` are the defaults (there's no way to add cflags via
        # config.mak), `-Wno-format-security` is the interesting
        # addition (without which gcc build fails in several places
        # due to `-Werror=format-security` being on for some reason)
        commonCFLAGS = "-g -O2 -Wno-format-security";
      in pkgs.stdenv.mkDerivation {
        name = "musl-cross-make";
        inherit system version;
        src = pkgs.fetchFromGitHub {
          owner = "richfelker";
          repo = "musl-cross-make";
          rev = version;
          sha256 = "sha256-FthfhZ+qGf2nWLICvjaO8fiP5+PYU7PqFCbPwXmJFes=";
        };

        nativeBuildInputs = [
          pseudoWget
        ];

        enableParallelBuilding = true;
        configurePhase = ''
          cat > config.mak <<EOF
          TARGET = ${target}
          OUTPUT = $out
          COMMON_CONFIG += CFLAGS="${commonCFLAGS}" CXXFLAGS="${commonCFLAGS}"
          EOF
        '';
      };
    in {
      devShells.default = (pkgs.buildFHSUserEnvBubblewrap {
        name = "vere-devenv";
        targetPkgs = pkgs: [
          toolchain
        ]
        ++ (with pkgs; [
          autoconf
          automake
          bazel_5
          binutils # for `nm`
          jdk11_headless
          libtool
          m4
        ]);
        extraBuildCommands = ''
          chmod +w usr
          mkdir -p usr/local
          # symlinking breaks Bazel inclusion checks, so have to copy
          cp -a --reflink=auto ${toolchain} usr/local/${target}
        '';
      }).env;
    };
  });
}
