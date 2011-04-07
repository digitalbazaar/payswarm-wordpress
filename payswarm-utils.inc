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

   // create new cookie value if it doesn't exist
   if($session === false)
   {
      $time_value = time();
      $random_value = rand(0, 100000);
      $session = sha1("$time_value$random_value");
      $update = true;
   }

   if($update)
   {
      // FIXME: if get_option("payswarm_ssl_on") then use https cookie, else
      // security is handled in a different way, so set cookie to non-https
      $secure = true;

      // update session cookie
      global $_SERVER;
      setcookie('payswarm-session', $session, time() + 3600, '/',
         $_SERVER['HTTP_HOST'], $secure);
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
   setcookie('payswarm-session', '', time() - 3600, '/',
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

/**
 * This function is called when the website using the payswarm plugin
 * receives an exception that it doesn't know how to handle.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_exception()
{
   global $post_id;

   // FIXME: Unfortunately, this generates a PHP Notice error for
   // WP_Query::$is_paged not being defined. Need to figure out which file
   // declares that variable.
   get_header();

   echo '
<div class="category-uncategorized">
  <h2 class="entry-title">Access Denied to PaySwarm Article</h2>
  <div class="entry-content">
    <p>
      Access to the article was denied because this website was not
      allowed to access your PaySwarm account. This usually happens because
      you did not allow this website to access your PaySwarm provider
      information.
    </p>

    <p><a href="' . get_permalink($post_id) .
      '">Go back to the article preview</a>.</p>
  </div>
</div>';

   get_footer();
}

?>
