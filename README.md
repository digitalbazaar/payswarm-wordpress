PaySwarm Plugin for WordPress
=============================

Introduction
------------

[PaySwarm][] enables people that create digital content such as blog posts,
music, film, episodic content, photos, virtual goods, and documents to
distribute their creations through their website and receive payment directly
from their fans and customers. If you have a passion for creating things on the
Web, or would like to support people doing great things - PaySwarm is for you.

The platform is an open, patent and royalty-free web standard that enables Web
browsers and Web devices to perform Universal Web Payment. PaySwarm fixes the
problems with rewarding people on the web - it reduces and nearly eliminates
transactional friction. It ensures that the people that you want to support are
automatically rewarded for their hard work.

We need Web-native payment technology that is designed to work with how the Web
works, not with how the banks and the credit card companies work. The
technology can be integrated directly into WordPress-based websites with
support for Drupal and other content management systems in the works. It has a
simple, well-defined API, like Twitter, that allows for universal payment on
the web.

Installing the WordPress Plugin
-------------------------------

This is a [WordPress][] plugin that implements a PaySwarm client. This plugin
can be installed in a normal WordPress 3.x site, allowing page authors to
charge a small fee for selected articles that they write.

If you downloaded the ZIP file:

1. Uncompress the ZIP file: unzip payswarm-X.Y.Z.zip.
2. Rename the 'payswarm-X.Y.Z' directory to 'payswarm'.
3. Place the directory into your WordPress wp-content/plugins/ directory.

Getting the Source Code
-----------------------

The source code for the PaySwarm WordPress plugin is available at:

http://github.com/digitalbazaar/payswarm-wordpress

Building the WordPress Package
------------------------------

Prerequisites: wget, git, make

To build the WordPress package for distribution, you must have cloned the git
repository and have the 'wget' and 'make' programs available. Type the
following command to build the package:

    make package

Working with the Source Code
----------------------------

Prerequisites: A working WordPress 3.0+ install, wget, git, make

You can clone the payswarm-wordpress git repository directly into your
WordPress wp-content/plugins/ directory and hack on the plugin directly by
doing the following:

    cd wp-content/plugins/
    git clone git://github.com/digitalbazaar/payswarm-wordpress.git payswarm
    cd payswarm
    make

[PaySwarm]: http://payswarm.com/
[WordPress]: http://wordpress.org/

