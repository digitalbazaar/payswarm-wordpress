<?php

/**
 * Gets the PaySwarm session that is being tracked on the site.
 *
 * @package payswarm
 * @since 1.0
 *
 * @return mixed the session ID for the active payswarm session or false if
 *         no such session exists.
 */
function payswarm_get_session()
{
   global $_COOKIE;
   $session = false;

   // check to see if the payswarm-session cookie exists
   if(array_key_exists('payswarm-session', $_COOKIE))
   {
      $session = $_COOKIE['payswarm-session'];
   }

   return $session;
}

/**
 * Creates a new PaySwarm session if one doesn't already exist. If one does
 * exist, it updates it if the "update" flag is set to true.
 *
 * The PaySwarm session is stored in a cookie 'payswarm-session'. This session
 * is used to associate the visitor with a PaySwarm Payment Token if one is
 * ever retrieved for the visitor.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param update true to update the PaySwarm session if already exists.
 *
 * @return string the session ID for the active PaySwarm session.
 */
function payswarm_create_session($update = true)
{
   global $_COOKIE;

   // try to get existing session
   $session = payswarm_get_session();

   // update cookie value if it doesn't exist
   if($session === false)
   {
      $update = true;
   }

   if($update)
   {
      $now = time();
      $timeout = 0;

      // no session exists, create new one
      if($session === false)
      {
         $random_value = rand(0, 100000);
         $session = sha1("$now$random_value");
      }
      // get timeout cookie
      else if(array_key_exists('payswarm-session-timeout', $_COOKIE))
      {
         $timeout = $_COOKIE['payswarm-session-timeout'];
      }

      // only update the cookie every 5 minutes since it requires a database
      // write (allows purging of expired sessions from the database)
      $now = time();
      if($now - 300 > $timeout)
      {
         // update session in database (updates expiration time)
         $timeout = $now + 3600;
         payswarm_database_update_session($session);

         // FIXME: if get_option("payswarm_ssl_on") then use https cookie, else
         // security is handled in a different way, so set cookie to non-https
         $secure = true;

         // update cookies
         global $_SERVER;
         setcookie(
            'payswarm-session', $session, $timeout, '/',
            $_SERVER['HTTP_HOST'], $secure);
         setcookie(
            'payswarm-timeout', $timeout, $timeout, '/',
            $_SERVER['HTTP_HOST'], $secure);
      }
   }

   return $session;
}

/**
 * Clears a PaySwarm session.
 *
 * Removes the 'payswarm-session' cookie by setting the value to empty
 * and setting the time an hour in the past.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_clear_session()
{
   // FIXME: if get_option("payswarm_ssl_on") then use https cookie, else
   // security is handled in a different way, so set cookie to non-https
   $secure = true;

   global $_SERVER;
   $expires = time() - 3600;
   setcookie('payswarm-session', '', $expires, '/',
      $_SERVER['HTTP_HOST'], $secure);
   setcookie('payswarm-timeout', '', $expires, '/',
      $_SERVER['HTTP_HOST'], $secure);
}

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

function payswarm_redirect($url)
{
   // FIXME: set header to redirect to URL in 5 seconds
   
   get_header();

   echo '
<div class="category-uncategorized">
  <h2 class="entry-title">Redirecting to PaySwarm provider</h2>
  <div class="entry-content">
    <p>
      We are redirecting you to your PaySwarm provider. Please wait...
    </p>
  </div>
</div>';

   get_footer();
}

/**
 * This function is called when the website using the payswarm plugin
 * receives an exception that it doesn't know how to handle.
 *
 * @param e the exception.
 * 
 * @package payswarm
 * @since 1.0
 */
function payswarm_exception($e)
{
   $response = json_decode($e->lastResponse);

   // FIXME: Unfortunately, this generates a PHP Notice error for
   // WP_Query::$is_paged not being defined. Need to figure out which file
   // declares that variable.
   get_header();

   echo '
<div class="category-uncategorized">
  <h2 class="entry-title">Problem with PaySwarm</h2>
  <div class="entry-content">
    <p>
      You have encountered a problem with the PaySwarm access process
      for this website.
    </p>
    <h3>Debug Information</h3>
    <p><pre>' . print_r($response, true) . '</pre></p>
    <p><pre>' . print_r($e, true) . '</pre></p>
  </div>
</div>';

   get_footer();
}

?>
