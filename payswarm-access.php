<?php

require_once('../../../wp-config.php');
require_once('payswarm-article.inc');

// see if a purchase response is available
if(isset($_POST['encrypted-message'])) {
  $response = $_POST['encrypted-message'];

  // make sure to remove magic quotes if in use
  if(get_magic_quotes_gpc()) {
    $response = stripcslashes($response);
  }
}

if(isset($response)) {
  payswarm_complete_purchase($response);
}
else {
  payswarm_init_purchase();
}

/* end of file, omit ?> */
