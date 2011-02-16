<?php

/**
 * Retrieves the current URL being served.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param boolean $force_https Force the resulting URL to be in HTTPS mode.
 *
 * @return string The current URL that is being served.
 */
function payswarm_get_current_url($force_https = false)
{
   $rval = 'http';

   // check whether we're in HTTP or HTTPS mode
   if((array_key_exists("HTTPS", $_SERVER) && $_SERVER["HTTPS"] === "on") ||
      $force_https)
   {
      $rval .= "s";
   }

   $rval .= "://";

   // add the server name, port (if non-standard), and the request URI
   if($_SERVER["SERVER_PORT"] !== "80")
   {
      $rval .= $_SERVER["SERVER_NAME"] . ":" . $_SERVER["SERVER_PORT"] .
         $_SERVER["REQUEST_URI"];
   }
   else
   {
      $rval .= $_SERVER["SERVER_NAME"] . $_SERVER["REQUEST_URI"];
   }

   return $rval;
}

/**
 * Forces the current page being displayed into SSL-mode.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_force_ssl()
{
   if(!array_key_exists("HTTPS", $_SERVER) || $_SERVER["HTTPS"] !== "on")
   {
      header('Location: ' . payswarm_get_current_url(true));
      exit();
   }
}
?>
