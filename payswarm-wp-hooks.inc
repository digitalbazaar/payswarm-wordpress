<?php

require_once('payswarm-config.inc');
require_once('payswarm-client.inc');

// add required payswarm protocol hooks
payswarm_add_hook('create_nonce', 'payswarm_wp_create_nonce');
payswarm_add_hook('check_nonce', 'payswarm_wp_check_nonce');
payswarm_add_hook('get_jsonld', 'payswarm_wp_get_jsonld');
payswarm_add_hook('post_jsonld', 'payswarm_wp_post_jsonld');

// add required payswarm storage hooks
payswarm_add_hook('get_public_key', 'payswarm_wp_get_public_key');
payswarm_add_hook('get_public_key_id', 'payswarm_wp_get_public_key_id');
payswarm_add_hook('get_private_key', 'payswarm_wp_get_private_key');
payswarm_add_hook('is_trusted_authority', 'payswarm_wp_is_trusted_authority');
payswarm_add_hook('store_key_pair', 'payswarm_wp_store_key_pair');
payswarm_add_hook('store_public_key_id', 'payswarm_wp_store_public_key_id');
payswarm_add_hook(
  'store_trusted_authority', 'payswarm_wp_store_trusted_authority');

// add optional payswarm cache hooks
payswarm_add_hook('cache_jsonld', 'payswarm_wp_cache_jsonld');
payswarm_add_hook('get_cached_jsonld', 'payswarm_wp_get_cached_jsonld');

/**
 * Creates a nonce to be given to a PaySwarm Authority so it can
 * be returned in a signed and encrypted message.
 *
 * @return string the nonce to use.
 */
function payswarm_wp_create_nonce() {
  // get client IP address
  $ip = $_SERVER['REMOTE_ADDR'];

  do {
    // generate a unique 20-digit pseudo-random nonce
    $rval =
      base_convert(mt_rand(), 10, 36) .
      base_convert(mt_rand(), 10, 36);
    $key = '_ps_nonce_' . $rval;

    // save the nonce in the database with a 15 minute timeout
    if(get_transient($key) === false) {
      set_transient($key, $ip, 60*15);
    }
  } while(get_transient($key) !== $ip);

  return $rval;
}

/**
 * Checks a message nonce. If the nonce exists and its associated IP address
 * matches the current IP address, then it is valid.
 *
 * @param string the nonce to check.
 *
 * @return true if the nonce is valid, false if not.
 */
function payswarm_wp_check_nonce($nonce) {
  // compare current IP w/nonce IP
  $nonce = '_ps_nonce_' . $nonce;
  $rval = ($_SERVER['REMOTE_ADDR'] === get_transient($nonce));

  // remove nonce
  delete_transient($nonce);

  return $rval;
}

/**
 * Gets JSON-LD from a URL.
 *
 * @param string $url the URL.
 *
 * @return string the retrieved JSON-LD.
 */
function payswarm_wp_get_jsonld($url) {
  $response = wp_remote_get($url, array(
    'sslverify' => PAYSWARM_VERIFY_SSL,
    'headers' => array('Accept' => 'application/ld+json')
  ));
  if(is_wp_error($response)) {
    throw new Exception("Could not GET url: '$url', " .
      $response->get_error_message());
  }
  return wp_remote_retrieve_body($response);
}

/**
 * Posts JSON-LD to a URL.
 *
 * @param string $data the JSON-LD data.
 * @param string $url the URL.
 *
 * @return string the retrieved JSON-LD.
 */
function payswarm_wp_post_jsonld($data, $url) {
  $response = wp_remote_post($url, array(
    'headers' => array(
      'Accept' => 'application/ld+json',
      'Content-Type' => 'application/ld+json',
      'Content-Length' => strlen($data),
      'User-Agent' => 'PaySwarm PHP Client/1.0'),
    'body' => $data,
    'sslverify' => PAYSWARM_VERIFY_SSL)
  );
  if(is_wp_error($response)) {
    throw new Exception("Could not POST to url: '$url', " .
      $response->get_error_message());
  }
  return wp_remote_retrieve_body($response);
}

/**
 * Gets the vendor's public key from storage.
 *
 * @return string the vendor's public key in PEM format or false.
 */
function payswarm_wp_get_public_key() {
  return get_option('_ps_public_key', false);
}

/**
 * Gets the vendor's public key ID (IRI) from storage.
 *
 * @return string the vendor's public key ID or false.
 */
function payswarm_wp_get_public_key_id() {
  return get_option('_ps_public_key_id', false);
}

/**
 * Gets the vendor's private key from storage.
 *
 * @return string the vendor's private key in PEM format or false.
 */
function payswarm_wp_get_private_key() {
  return get_option('_ps_private_key', false);
}

/**
 * Returns whether or not the given PaySwarm Authority Identity is trusted.
 *
 * @return boolean true if trusted, false if not.
 */
function payswarm_wp_is_trusted_authority($id) {
  return get_option('_ps_tpa_' . md5($id), false);
}

/**
 * Stores the vendor's key pair.
 *
 * @param string $public_pem the public key in PEM format.
 * @param string $private_pem the private key in PEM format.
 */
function payswarm_wp_store_key_pair($public_pem, $private_pem) {
  update_option('_ps_public_key', $public_pem);
  update_option('_ps_private_key', $private_pem);
}

/**
 * Stores the vendor's public key ID (IRI).
 *
 * @param string $id the vendor's public key ID.
 */
function payswarm_wp_store_public_key_id($id) {
  update_option('_ps_public_key_id', $id);
}

/**
 * Stores that a particular PaySwarm Authority is trusted.
 *
 * @param string $id the authority's ID.
 */
function payswarm_wp_store_trusted_authority($id) {
  update_option('_ps_tpa_' . md5($id), true);
}

/**
 * Caches a JSON-LD object.
 *
 * @param string $id the ID (IRI) of the object.
 * @param stdClass $obj the JSON-LD object.
 * @param secs the maximum number of seconds to cache.
 */
function payswarm_wp_cache_jsonld($id, $obj, $secs) {
  $id = '_ps_cache_' . md5($id);
  set_transient($id, $obj, $secs);
}

/**
 * Retrieves a cached JSON-LD object.
 *
 * @param string $id the ID (IRI) of the object.
 *
 * @return stdClass the JSON-LD object or false.
 */
function payswarm_wp_get_cached_jsonld($id) {
  $id = '_ps_cache_' . md5($id);
  return get_transient($id);
}

/* end of file, omit ?> */
