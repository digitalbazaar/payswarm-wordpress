VERSION=0.9.1
PACKAGE=payswarm-${VERSION}

.PHONY: prerequisites

prerequisites: jsonld.php
	@(test -n "$(shell which wget)" || echo "WARNING: wget is not installed, retrieving jsonld.php will fail.")
	@(test -n "$(shell which git)" || echo "WARNING: git is not installed, package building will fail.")

jsonld.php:
	@echo -n "Retrieving jsonld.php from github.com... "
	@wget -q -nc https://raw.github.com/digitalbazaar/php-json-ld/master/jsonld.php
	@echo "done."

package: jsonld.php
	@(test -d .git || echo "ERROR: No git repository found, package building will fail.")
	mkdir -p payswarm
	cp -a --parents $(shell git ls-files) jsonld.php payswarm
	rm payswarm/Makefile payswarm/.gitignore
	zip -q -r ${PACKAGE}.zip payswarm
	rm -rf payswarm

