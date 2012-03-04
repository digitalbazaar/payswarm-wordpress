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

    // update cookies
    global $_SERVER;
    setcookie(
      'payswarm-session', $session['id'], $timeout, '/', $_SERVER['HTTP_HOST']);
    setcookie(
      'payswarm-timeout', $timeout, $timeout, '/', $_SERVER['HTTP_HOST']);
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
    global $_SERVER;
    $expires = time() - 3600;
    setcookie('payswarm-session', '', $expires, '/', $_SERVER['HTTP_HOST']);
    setcookie('payswarm-timeout', '', $expires, '/', $_SERVER['HTTP_HOST']);

    // clear authority host URL cookie as well
    payswarm_clear_authority_host_cookie();
  }
}

/**
 * Sets the payswarm-authority-host cookie.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $host the customer's PaySwarm Authority host.
 */
function payswarm_set_authority_host_cookie($host) {
  global $_SERVER;
  $timeout = time() + 3600;
  setcookie(
    'payswarm-authority-host', $host, $timeout, '/', $_SERVER['HTTP_HOST']);
}

/**
 * Gets the customer's PaySwarm Authority host that is being tracked on the
 * site.
 *
 * @package payswarm
 * @since 1.0
 *
 * @return mixed the customer's PaySwarm Authority host or false if no
 *         such value exists.
 */
function payswarm_get_authority_host_cookie() {
  global $_COOKIE;
  $host = false;

  // check to see if the payswarm-authority-host cookie exists
  if(array_key_exists('payswarm-authority-host', $_COOKIE)) {
    $host = $_COOKIE['payswarm-authority-host'];
  }

  return $host;
}

/**
 * Clears the payswarm-authority-host cookie.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_clear_authority_host_cookie() {
  if(!headers_sent()) {
    global $_SERVER;
    $expires = time() - 3600;
    setcookie('payswarm-authority-host', '',
      $expires, '/', $_SERVER['HTTP_HOST']);
  }
}

/* end of file, omit ?> */
