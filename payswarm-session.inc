<?php

define('PAYSWARM_SESSION_TIMEOUT', 3600);

// get session on plugin initialization to refresh/clear it
add_action('init', 'payswarm_init_session');
function payswarm_init_session() {
  payswarm_get_session();
}

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

  // expiration interval
  $timeout = PAYSWARM_SESSION_TIMEOUT;

  // try to get existing session
  $session = payswarm_get_session($identity_id);
  $now = time();

  // no session exists, create a new one
  if($session === false) {
    // session ID length must be <= 32 to use transient API below
    $now = time();
    $random_value = mt_rand(0, 100000);
    $session = array(
      'id' => md5("$now$random_value"),
      'ip' => $_SERVER['REMOTE_ADDR'],
      'identity_id' => $identity_id,
      'expires' => $now + $timeout,
      'last_update' => 0);
  }

  // only update cookie if a minute has passed since last_update to
  // throttle writes (timeout is 0 for new sessions, so will always write)
  if($now - 60 > $session['last_update']) {
    // update session, auto-clear from DB in 24 hours
    // 24 hours allows time to show "You've been signed out" warning
    $expires = $now + $timeout;
    $session['expires'] = $expires;
    $session['last_update'] = $now;
    set_transient('ps_sess_' . $session['id'], $session, $timeout + 24*3600);

    // update cookie (client cookie is browser-session based)
    global $_SERVER;
    setcookie('payswarm-session', $session['id'], 0, '/');
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
 * @param boolean $return_expired if true and a session exists but it is
 *          expired, return the timestamp for when the session expired instead
 *          of returning false.
 *
 * @return mixed either the active PaySwarm session, the expiration timestamp
 *           for any expired session if $return_expired is true (false
 *           otherwise), or false if no such session exists.
 */
function payswarm_get_session($identity_id = null, $return_expired = false) {
  global $_COOKIE;
  $session = false;

  // check to see if the payswarm-session cookie exists
  if(array_key_exists('payswarm-session', $_COOKIE)) {
    $session_id = $_COOKIE['payswarm-session'];
    $session = get_transient('ps_sess_' . $session_id);
    if($session !== false) {
      // ensure client IP address and identity ID match
      $ip = $_SERVER['REMOTE_ADDR'];
      if($session['ip'] !== $ip ||
        ($identity_id !== $session['identity_id'] &&
        $identity_id !== null)) {
        $session = false;
      } else {
        // check expiration
        $now = time();
        $expires = isset($session['expires']) ? $session['expires'] : $now;
        if($expires <= $now) {
          $session = $return_expired ? $expires : false;
        }
      }
    }

    if($session === false || is_numeric($session)) {
      // invalid session, clear cookie only (not server session)
      payswarm_clear_session_cookie();
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
    $past = time() - 3600;
    setcookie('payswarm-session', '', $past, '/');

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
  setcookie('payswarm-authority-host', $host, 0, '/');
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
    $past = time() - 3600;
    setcookie('payswarm-authority-host', '', $past, '/');
  }
}

/* end of file, omit ?> */
