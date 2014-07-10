<?php

// Note: Do not move this code after the inclusion of wp-config. It adds
// slashes to POST data even if magic quotes is off.

// see if a purchase response is available
if(isset($_POST['encrypted-message'])) {
  $response = $_POST['encrypted-message'];

  // make sure to remove magic quotes if in use
  if(get_magic_quotes_gpc()) {
    $response = stripcslashes($response);
  }
}

require_once('../../../wp-config.php');
require_once('payswarm-article.inc');

if(isset($response)) {
  payswarm_complete_purchase($response);
} else {
  payswarm_init_purchase();
}

/* end of file, omit ?> */
