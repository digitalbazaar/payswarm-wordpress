<?php

/**
 * Creates a new PaySwarm session if one doesn't already exist. If one does
 * exist, it will be updated with the given identity ID if it hasn't already
 * been set.
 *
 * The PaySwarm session is stored in a cookie 'payswarm-session'. This session
 * is used to associate the visitor with a PaySwarm identity if one is
 * ever retrieved for the visitor.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $identity_id the identity IRI to associate with the session.
 *
 * @return string the session ID for the active PaySwarm session.
 */
function payswarm_create_session($identity_id = null) {
  global $_COOKIE;

  // try to get existing session
  $session = payswarm_get_session($identity_id);
  $timeout = 0;

  // no session exists, create a new one
  if($session === false) {
    // session ID length must be <= 32 to use transient API below
    $now = time();
    $random_value = mt_rand(0, 100000);
    $session = array(
      'id' => md5("$now$random_value"),
      'ip' => $_SERVER['REMOTE_ADDR'],
      'identity_id' => $identity_id);
  }
  // get timeout cookie value
  else if(array_key_exists('payswarm-session-timeout', $_COOKIE)) {
    $timeout = $_COOKIE['payswarm-session-timeout'];
  }

  // only update cookie every 5 minutes since it requires a write
  // (timeout is 0 for new sessions, so always write in that case)
  $now = time();
  if($now - 300 > $timeout) {
    // get client IP address
    $ip = $_SERVER['REMOTE_ADDR'];

    // update session (updates expiration time and identity ID)
    $timeout = $now + 3600;
    set_transient('ps_sess_' . $session['id'], $session, 3600);

    // set cookie security based on page
    $secure = ($_SERVER['HTTPS'] === 'on');

    // update cookies
    global $_SERVER;
    setcookie(
      'payswarm-session', $session['id'], $timeout, '/',
      $_SERVER['HTTP_HOST'], $secure);
    setcookie(
      'payswarm-timeout', $timeout, $timeout, '/',
      $_SERVER['HTTP_HOST'], $secure);
   }

   return $session;
}

/**
 * Gets the PaySwarm session that is being tracked on the site.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $identity_id the identity IRI to check against the session,
 *          null for no check.
 *
 * @return mixed the active PaySwarm session or false if no such session
 *         exists.
 */
function payswarm_get_session($identity_id = null) {
  global $_COOKIE;
  $session = false;

  // check to see if the payswarm-session cookie exists
  if(array_key_exists('payswarm-session', $_COOKIE)) {
    $session_id = $_COOKIE['payswarm-session'];
    $session = get_transient('ps_sess_' . $session_id);
    if($session !== false) {
      // ensure client IP address and identity ID match
      $ip = $_SERVER['REMOTE_ADDR'];
      if($session['ip'] !== $ip or
        ($identity_id !== $session['identity_id'] and
        $identity_id !== null)) {
        // invalid session, clear cookie only
        payswarm_clear_session_cookie();
        $session = false;
      }
    }
  }

  return $session;
}

/**
 * Clears a PaySwarm session. Removes cookies and server-side state.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_clear_session() {
  // clear existing stored session
  $session = payswarm_get_session();
  if($session !== false) {
    delete_transient('ps_sess_' . $session['id']);
  }

  // clear cookie
  payswarm_clear_session_cookie();
}

/**
 * Clears a PaySwarm session cookie (does not clear the server-side session
 * state).
 *
 * Removes any session cookies by setting their values to empty and their
 * expiration time to an hour in the past.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_clear_session_cookie() {
  if(!headers_sent()) {
    // set cookie security based on page
    $secure = ($_SERVER['HTTPS'] === 'on');

    global $_SERVER;
    $expires = time() - 3600;
    setcookie('payswarm-session', '', $expires, '/',
      $_SERVER['HTTP_HOST'], $secure);
    setcookie('payswarm-timeout', '', $expires, '/',
      $_SERVER['HTTP_HOST'], $secure);

    // clear payment URL cookie as well
    payswarm_clear_payment_url_cookie();
  }
}

/**
 * Sets the payment-url cookie.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $payment_url the customer's PaySwarm Authority payment URL.
 */
function payswarm_set_payment_url_cookie($payment_url) {
  global $_SERVER;
  $timeout = time() + 3600;
  setcookie(
    'payment-url', $payment_url, $timeout, '/',
    $_SERVER['HTTP_HOST'], $_SERVER['HTTPS'] === 'on');
}

/**
 * Gets the value of the customer's PaySwarm Authority payment URL that is
 * being tracked on the site.
 *
 * @package payswarm
 * @since 1.0
 *
 * @return mixed the customer's PaySwarm Authority payment URL or false if no
 *         such value exists.
 */
function payswarm_get_payment_url_cookie() {
  global $_COOKIE;
  $payment_url = false;

  // check to see if the payment-url cookie exists
  if(array_key_exists('payment-url', $_COOKIE)) {
    $payment_url = $_COOKIE['payment_url'];
  }

  return $payment_url;
}

/**
 * Clears the payment-url cookie.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_clear_payment_url_cookie() {
  if(!headers_sent()) {
    global $_SERVER;
    $expires = time() - 3600;
    setcookie('payment-url', '', $expires, '/',
      $_SERVER['HTTP_HOST'], $_SERVER['HTTPS'] === 'on');
  }
}

/* end of file, omit ?> */
