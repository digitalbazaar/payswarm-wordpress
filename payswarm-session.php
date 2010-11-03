<?php
// ensure that the PaySwarm session is being tracked
add_action('sanitize_comment_cookies', 'payswarm_check_session');

function payswarm_check_session()
{
   global $_COOKIE;
   $id = 0;

   if(array_key_exists('payswarm-session', $_COOKIE))
   {
      $id = $_COOKIE['payswarm-session'];
   }
   else
   {
      $timeVal = time();
      $randomVal = rand(0, 100000);
      $id = sha1('$timeVal$randomVal');
      $ptok = array(
         'id' => $id, 'state' => 'initializing', 
         'token' => '', 'secret' => '', 'amount' => '');
   }
   setcookie('payswarm-session', $id, time() + 3600, '/', site_url(), true);
}
?>

