<?php

// Note: Do not move this code after the inclusion of wp-config. It adds
// slashes to POST data even if magic quotes is off.

// see if a registration response is available
if(isset($_POST['encrypted-message'])) {
  $response = $_POST['encrypted-message'];

  // make sure to remove magic quotes if in use
  if(get_magic_quotes_gpc()) {
    $response = stripcslashes($response);
  }
}

require_once('../../../wp-config.php');
require_once('payswarm-admin.inc');

if(!current_user_can('manage_options')) {
  wp_die(__('Access denied.'));
}

if(isset($response)) {
  payswarm_complete_registration($response);
}
else {
  payswarm_init_registration();
}

/* end of file, omit ?> */
