<?php

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
      $time_value = time();
      $random_value = rand(0, 100000);
      $id = sha1("$time_value$random_value");
      $ptok = array(
         'id' => $id, 'state' => 'initializing', 
         'token' => '', 'secret' => '', 'amount' => '');
   }
   setcookie('payswarm-session', $id, time() + 3600, '/', '.sites.local', true);
}
?>
