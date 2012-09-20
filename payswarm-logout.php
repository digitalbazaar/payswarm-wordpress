<?php

require_once('../../../wp-config.php');
require_once('payswarm-session.inc');

payswarm_clear_session();
wp_redirect(home_url($_GET['redirect']));

/* end of file, omit ?> */
