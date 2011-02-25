<?php

/**
 * Checks to ensure that a PaySwarm session is being tracked on the site.
 *
 * Checks to see if a cookie called 'payswarm-session' exists in HTTPS mode,
 * if it doesn't, one is created. This session is used to associate the visitor
 * with a PaySwarm Payment Token if one is ever retrieved for the visitor.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param boolean update if set to true, updates the cookie value.
 *
 * @return string the session ID for the active payswarm session.
 */
function payswarm_check_session($update = true)
{
   global $_COOKIE;
   $rval = 0;

   // check to see if the payswarm-session cookie exists
   if(array_key_exists('payswarm-session', $_COOKIE))
   {
      $rval = $_COOKIE['payswarm-session'];
   }
   else
   {
      // generate a new random cookie if the cookie already exists
      $time_value = time();
      $random_value = rand(0, 100000);
      $rval = sha1("$time_value$random_value");
   }

   // update the cookie value if it needs to be updated
   if($update)
   {
      global $_SERVER;
      setcookie('payswarm-session', $rval, time() + 3600, '/',
         $_SERVER['HTTP_HOST'], true);
   }

   return $rval;
}
?>
