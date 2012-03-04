<?php

define('PAYSWARM_PRODUCTION', false);

define('PAYSWARM_PLUGIN_URL', site_url() . '/wp-content/plugins/' .
  str_replace(basename(__FILE__), '', plugin_basename(__FILE__)));

define('PAYSWARM_VERIFY_SSL', PAYSWARM_PRODUCTION);

if(PAYSWARM_PRODUCTION) {
  // FIXME: this should be a commercial site
  define('PAYSWARM_AUTHORITY_HOST', 'dev.payswarm.com');
  define('PAYSWARM_AUTHORITY_URL', 'https://' . PAYSWARM_AUTHORITY_HOST);
}
else {
  define('PAYSWARM_AUTHORITY_HOST', 'dev.payswarm.com');
  define('PAYSWARM_AUTHORITY_URL', 'https://' . PAYSWARM_AUTHORITY_HOST);
}

/* end of file, omit ?> */
