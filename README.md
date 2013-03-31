PaySwarm Plugin for WordPress
=============================

Introduction
------------

[PaySwarm][] enables people who create digital content such as blog posts,
music, film, episodic content, photos, virtual goods, and documents to
distribute their creations through their website and receive payment directly
from their fans and customers. If you have a passion for creating things on the
Web or would like to support people doing great things - PaySwarm is for you.

The platform is an open, patent and royalty-free web standard that enables Web
browsers and Web devices to perform Universal Web Payment. PaySwarm fixes the
problems with rewarding people on the web - it reduces and nearly eliminates
transactional friction. It ensures that the people who you want to support are
automatically rewarded for their hard work.

This technology can be integrated directly into WordPress-based websites with
support for Drupal and other content management systems in the works. It has a
simple, well-defined API, like Twitter, that allows for universal payment on
the web.

WordPress Plugin
----------------

This is a [WordPress][] plugin that implements a PaySwarm client. This plugin
can be installed in a normal WordPress 3.x site, allowing page authors to
charge a small fee for selected posts that they write.

Once the plugin is installed, the author of a post has a few options that
they can use to markup the post. They can indicate the pieces of the post
that must be paid for, what to display for posts that haven't been paid for,
and where to place the button that initiates the payment process.

A typical post will look like this:

```html
This is free content; anyone can see it.

BEGIN_PAYSWARM_PAID_CONTENT

This is paid content; only people who have paid can see it.
```

An author can also provide text that will only be shown for people who
haven't paid for the post:

```html
This is free content; anyone can see it.

BEGIN_PAYSWARM_UNPAID_ONLY_CONTENT
This is unpaid-only content; it will be hidden once the post has been paid for.
END_PAYSWARM_UNPAID_ONLY_CONTENT

BEGIN_PAYSWARM_PAID_CONTENT
This is paid content; only people who have paid can see it.
END_PAYSWARM_PAID_CONTENT
```

The author can also decide where they want to place the button
that initiates the purchase as well as a short piece of text that
will be shown beside the access button:

```html
This is free content; anyone can see it.

PAYSWARM_ACCESS_BUTTON Fund my coffee addiction to create more posts like this.

BEGIN_PAYSWARM_PAID_CONTENT
This is paid content; only people who have paid can see it.
END_PAYSWARM_PAID_CONTENT
```

If no access button markup is specified but paid content markup is, then
an access button with some default text will appear at the end of the post.

The price of the post, the license that is granted upon purchase, 
and other post-specific values can be changed on a per-post basis.

Pre-requisites
--------------

Before you install this plugin, you will need the following software:

1. A [WordPress][] website (version 3.x or greater)
2. [PHP][] (version 5.3 or greater)
3. [wget][]
4. [make][]

Basic Development Install
-------------------------

Go to your WordPress installation and run the following commands:

    cd wp-content/plugins/
    git clone git://github.com/digitalbazaar/payswarm-wordpress.git payswarm
    cd payswarm
    make

PaySwarm Plugin Setup
---------------------

To configure your new PaySwarm plugin for WordPress, you must do the
following steps:

1. [Register for a PaySwarm account][].
2. Login as administrator on your WordPress website.
3. Go to the PaySwarm Plugin page (select Plugins -> PaySwarm).
4. Click the "Register this site" button.
5. Click the "Add Identity" button.
6. Enter the name of your WordPress website (e.g. "Good Food").
7. Select "Vendor" for the type of Identity.
8. Enter the address for your website (e.g. "http://foo.bar.com/").
8. Enter a short description of your website (e.g. "Good Food strives to discover and share delicous recipes.").
9. Enter the name of your new WordPress website Financial Account (e.g. "Blogging Revenue").
10. Select "Public" for the type of Account Visibility.
11. Click the "Add" button.
12. Enter the name of your Access Key Label (e.g. "Good Food Vendor Key 2012-09-25")
13. Click the "Register" button.
14. If there are no errors when you get back to the WordPress plugin page, registration was successful.
15. Go to the PaySwarm Plugin Settings page (select Settings -> PaySwarm).
16. Set the default price for posts (e.g. "0.05")
17. Click the "Save Changes" button.

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
[PHP]: http://www.php.net/
[wget]: http://www.gnu.org/software/wget/
[make]: http://www.gnu.org/software/make/
[Register for a PaySwarm Account]: https://dev.payswarm.com/profile/create
