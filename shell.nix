{ pkgs ? import <nixpkgs> {} }: pkgs.mkShell {
	nativeBuildInputs = with pkgs.buildPackages; [
		libgcc
		gdb
		valgrind
		unixtools.xxd
	];
}
