<?php

require_once('../../../wp-config.php');
require_once('payswarm-admin.inc');

if(!is_admin() or
  (is_multisite() and !is_super_admin())) {
  wp_die(__('You must be an admin to register.'));
}

// see if a registration response is available
if(isset($_POST['encrypted-message'])) {
  $response = $_POST['encrypted-message'];

  // make sure to remove magic quotes if in use
  if(get_magic_quotes_gpc()) {
    $response = stripcslashes($response);
  }
}

if(isset($response)) {
  payswarm_complete_registration($response);
}
else {
  payswarm_init_registration();
}

/* end of file, omit ?> */
