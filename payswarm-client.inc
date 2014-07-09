<?php

/*
PHP implementation of the PaySwarm client API.
Version: 0.1.1

@author Dave Longley

New BSD License (3-clause)
Copyright (c) 2010-2013, Digital Bazaar, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

Neither the name of Digital Bazaar, Inc. nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL DIGITAL BAZAAR BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

require_once('jsonld.php');
require_once('bedrock.php');

/*

PAYSWARM CLIENT API
-------------------

The PaySwarm Client API allows vendors to register with a PaySwarm Authority,
sign listings for the items they wish to sell, and receive payments from
their customers.

INSTRUCTIONS
------------

First, implement all of the required hooks. Various hooks will be triggered
when making calls to the API. Most of the hooks involve providing the API with
a custom mechanism for doing HTTP GET/POST and storing/retrieving data from
a database. It is also highly recommended that the optional cache hooks be
implemented to prevent excessive network traffic when looking up PaySwarm
Authority configurations and public keys. To implement a hook, simply write
a function that takes the appropriate parameters and returns the appropriate
values. Then pass the hook name and the name of the custom function to
'payswarm_add_hook'. Look below for the specific hooks that must be
implemented.

Next, use the API to register as a vendor on the PaySwarm Authority of
choice, sign listings, and accept payments via customers' chosen PaySwarm
Authorities.

1. Add the PaySwarm Authorities that should be trusted by calling:

  payswarm_add_trusted_authority('trustedauthority:port');

  In this version of the API, any customer's PaySwarm Authority that the vendor
  would like to do business with must be manually added. The vendor's chosen
  PaySwarm Authority will be automatically added during the registration
  step.

2. Register as a vendor by calling:

  $url = payswarm_get_register_vendor_url(
    'myauthority:port',
    'http://myserver/myregistercallbackurl.php');

  The first parameter is the host and port of the PaySwarm Authority to
  register with. The second is a callback URL that will receive the result of
  the registration as POST data.

  Direct the vendor to the URL so that they can complete the registration
  process. Once the registration process is complete, the vendor's browser
  will POST the registration result to the callback URL provided.

3. On the callback page, get the POST value 'encrypted-message' and pass it
  to register the vendor:

  payswarm_register_vendor($_POST['encrypted-message']);

  To handle PHP magic quotes do this:

  if(get_magic_quotes_gpc()) {
    $encrypted_message = stripcslashes($_POST['encrypted-message']);
  }

  If no exceptions are thrown, registration is complete. The returned object
  is the PaySwarm Vendor's Preferences, including the Financial Account ID
  to use in Listings.

5. Create a JSON-LD PaySwarm Asset and Listing. When listing an Asset, its
  unique hash must be in the Listing. To generate an asset hash call:

  payswarm_hash($asset);

4. Sign a listing. Create a JSON-LD PaySwarm Listing and then sign it:

  payswarm_sign($listing);

  Display the listing information; the use of RDFa is recommended. Depending
  on the application's needs, it is sometimes a good idea (or a requirement)
  to regenerate signatures when the vendor's public key is changed.

  Note: A Listing also contains a License for the Asset. If the application
  knows the ID (IRI) of the License to use but not the License hash, and it
  does not have the necessary parser to obtain the License information from
  its ID, it may use the PaySwarm Authority's license service to cache and
  retrieve the License by its ID. Then payswarm_hash($license) can be called
  on the result to produce its hash.

5. When a customer indicates that they want to purchase the Asset in a
  Listing, call:

  $url = payswarm_get_purchase_url(
    'customersauthority:port',
    $listing_id,
    $listing_hash,
    'https://myserver/mypurchasecallbackurl.php');

  To get a URL to redirect the customer to their PaySwarm Authority to
  complete the purchase. The last parameter is a callback URL that will
  receive the result of the purchase as POST data.

  If the customer has previously completed a purchase and the response
  indicated that they set up a budget to handle automated purchases in the
  future, then an automated purchase can be attempted by calling:

  payswarm_purchase(
    'customersauthority:port',
    'https://customersauthority:port/i/customer',
    $listing_id,
    $listing_hash);

  In this version of the API, it is the responsibility of the application to
  determine the customer's PaySwarm Authority (usually by asking). A listing
  hash can be generated by calling:

  payswarm_hash($listing);

  To get the JSON-LD receipt from a purchase, call:

  payswarm_get_receipt($encrypted_message);

  Where $encrypted_message is either the result of a POST to the purchase
  callback or the result of the payswarm_purchase() call.

  The receipt will indicate the ID and hash of the Asset purchased as well
  as the ID and hash of the License for the Asset.

*/

// TODO: deprecate hooks everywhere, pass params instead, use bedrock API
// directly

// hook API
$payswarm_hooks = array();

// set jsonld document loader
jsonld_set_document_loader('payswarm_jsonld_load_document');

/**
 * Adds a hook. To add a hook, pass the name of the hook (eg: create_nonce) and
 * the user-defined function name to be called. Hooks are permitted to throw
 * exceptions as are any PaySwarm client API calls. API calls should be
 * wrapped in try/catch blocks as appropriate.
 *
 * Required protocol hooks:
 *
 * create_nonce(): Creates and stores a nonce that is to be given to a
 *   PaySwarm Authority so it can be returned in a signed and encrypted
 *   message.
 *
 * check_nonce($nonce): Checks a nonce previously created by create_nonce and
 *   removes it from storage. Returns true if the nonce is valid, false if not.
 *
 * Required storage hooks:
 *
 * get_public_key(): Returns the vendor's public key in PEM format.
 *
 * get_public_key_id(): Returns the ID (IRI) for the vendor's public key.
 *
 * get_private_key(): Returns the vendor's private key in PEM format.
 *
 * is_trusted_authority($id): Returns true if the given identity (IRI) is a
 *   trusted PaySwarm Authority, false if not.
 *
 * store_key_pair($public_pem, $private_pem): Stores the vendor's key pair.
 *
 * store_public_key_id($id): Stores the vendor's public key ID (IRI).
 *
 * store_trusted_authority($id): Stores the ID (IRI) of a trusted
 *   PaySwarm Authority.
 *
 * Optional hooks:
 *
 * get_jsonld($url): Returns the JSON-encoded body of an HTTP GET where
 *   the expected content-type is 'application/ld+json'.
 *
 * post_jsonld($url, $data): HTTP POSTs the given JSON-LD data to the given
 *   URL and returns the response body.
 *
 * cache_jsonld($url, $obj, $secs): Caches a JSON-LD object. The url (ID) for
 *   the object is given and the maxmimum number of seconds to cache.
 *
 * get_cached_jsonld($url): Gets a JSON-LD object from cache. Returns the
 *   object or false.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $hook the name of the hook.
 * @param string $func the name of the function to call.
 */
function payswarm_add_hook($hook, $func) {
  global $payswarm_hooks;
  $payswarm_hooks[$hook] = $func;
}

/**
 * Gets the default payswarm JSON-LD context URL.
 *
 * @package payswarm
 * @since 1.0
 *
 * @return string the default payswarm JSON-LD context URL.
 */
function payswarm_get_default_jsonld_context_url() {
  return 'https://w3id.org/payswarm/v1';
}

/**
 * Creates a default payswarm JSON-LD context.
 *
 * @package payswarm
 * @since 1.0
 *
 * @return stdClass the default payswarm JSON-LD context.
 */
function payswarm_create_default_jsonld_context() {
  $ctx = file_get_contents('./payswarm-context.jsonld');
  return jsonld_decode($ctx);
}

/**
 * Retrieves a JSON-LD object over HTTP.
 *
 * @param string $url the URL to HTTP GET.
 * @param assoc $options the options to use.
 *          [cache] a cache interface to use.
 *          [get_jsonld($url, $options)] an alternate get function.
 *          See bedrock_http_request for HTTP options.
 *
 * @return stdClass the JSON-LD object.
 */
function payswarm_get_jsonld($url, $options=array()) {
  global $payswarm_hooks;
  global $payswarm_jsonld_cache;

  if(!isset($options['cache'])) {
    $options['cache'] = $payswarm_jsonld_cache;
  }
  if(!isset($options['get_jsonld']) && isset($payswarm_hooks['get_jsonld'])) {
    $options['get_jsonld'] = $payswarm_hooks['get_jsonld'];
  }

  return bedrock_get_jsonld($url, $options);
}

/**
 * HTTP POSTs a JSON-LD object.
 *
 * @param string $url the URL to HTTP POST to.
 * @param stdClass $obj the JSON-LD object.
 * @param assoc $options the options to use.
 *          See bedrock_http_request for HTTP options.
 *
 * @return stdClass the JSON-LD response.
 */
function payswarm_post_jsonld($url, $obj, $options=array()) {
  global $payswarm_hooks;

  if(!isset($options['post_jsonld']) && isset($payswarm_hooks['post_jsonld'])) {
    $options['post_jsonld'] = $payswarm_hooks['post_jsonld'];
  }

  return bedrock_post_jsonld($url, $obj, $options);
}

/**
 * Retrieves a JSON-LD document URL, returning the remote document.
 *
 * @param string $url the URL for the JSON-LD document.
 *
 * @return stdClass the JSON-LD RemoteDocument object.
 */
function payswarm_jsonld_load_document($url) {
  global $payswarm_jsonld_cache;

  // FIXME: hack until https://w3id.org/payswarm/v1 is ready
  if($url === 'https://w3id.org/payswarm/v1') {
    return (object)array(
      'contextUrl' => null,
      'documentUrl' => $url,
      'document' => (object)array(
        '@context' => payswarm_create_default_jsonld_context()));
  }

  // use cache if available
  $rval = $payswarm_jsonld_cache->get($url);
  if($rval === false) {
    // do secure retrieval of URL
    $rval = jsonld_default_secure_document_loader($url);
    if($cache) {
      $payswarm_jsonld_cache->set($url, $rval);
    }
  }

  return $rval;
}

/**
 * Creates a nonce for a secure message.
 *
 * @return string the nonce.
 */
function payswarm_create_nonce() {
  global $payswarm_hooks;
  return call_user_func($payswarm_hooks['create_nonce']);
}

/**
 * Checks the nonce from a secure message.
 *
 * @param string $nonce the nonce.
 * @param assoc $options the options to use.
 *
 * @return true if the nonce is valid, false if not.
 */
function payswarm_check_nonce($nonce, $options) {
  global $payswarm_hooks;
  return call_user_func($payswarm_hooks['check_nonce'], $nonce);
}

/**
 * Generates a hash of the JSON-LD encoded data.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param stdClass $obj the JSON-LD object to hash.
 *
 * @return the SHA-1 hash of the encoded JSON data.
 */
function payswarm_hash($obj) {
  return bedrock_hash($obj);
}

/**
 * Signs a JSON-LD object, adding a signature field to it. If a signature
 * date is not provided then the current date will be used.
 *
 * @param stdClass $obj the JSON-LD object to sign.
 * @param assoc $options the options to use.
 *          key the private key in PEM format.
 *          keyId the ID of the public key associated with the private key.
 *          [date] the ISO8601-formatted signature creation date.
 *          [domain] a domain to restrict the signature to.
 *          [nonce] a nonce to use.
 */
function payswarm_sign($obj, $options) {
  return bedrock_sign($obj, $options);
}

/**
 * Verifies a JSON-LD digitally signed object.
 *
 * @param stdClass $obj the JSON-LD object to verify.
 * @param assoc $options the options to use.
 *          [check_nonce($nonce, $options)] a function to call to check to see
 *            if the nonce (null if none) used to sign the message is valid.
 *          [check_domain($domain, $options)] a function to call to check to
 *            see if the domain used (null if none) is valid.
 *          [check_key($key, $options)] a function to call to check to see if
 *            the key used to sign the message is trusted.
 *          [check_key_owner($owner, $key, $options)] a function to call to
 *            check to see if the key's owner is trusted.
 *          See bedrock_http_request for HTTP options.
 *
 * @return true if verified, false if not (exception thrown).
 */
function payswarm_verify($obj, $options=array()) {
  global $payswarm_jsonld_cache;

  // set defaults
  if(!isset($options['cache'])) {
    $options['cache'] = $payswarm_jsonld_cache;
  }
  if(!isset($options['check_nonce'])) {
    $options['check_nonce'] = 'payswarm_check_nonce';
  }
  if(!isset($options['check_key_owner'])) {
    $options['check_key_owner'] = 'payswarm_check_key_owner';
  }

  return bedrock_verify($obj, $options);
}

/**
 * Ensures a key owner is a trusted authority.
 *
 * @param string $owner the ID of the key owner.
 * @param stdClass $key the public key.
 *
 * @return true if the owner is trusted, false if not.
 */
function payswarm_check_key_owner($owner, $key) {
  global $payswarm_hooks;
  $trusted = call_user_func($payswarm_hooks['is_trusted_authority'], $owner);
  if(!$trusted) {
    throw new Exception('PaySwarm Security Exception: ' .
      'The public key owner is not a trusted.');
  }
}

/**
 * Decrypts a JSON-encoded, encrypted, digitally-signed JSON-LD message
 * from a PaySwarm Authority.
 *
 * @param string $encrypted the message to decrypt.
 * @param string $key the PEM-encoded private key to decrypt the message.
 * @param assoc $options see bedrock_verify for verification options.
 *
 * @return stdClass the decrypted JSON-LD object (exception thrown on error).
 */
function payswarm_decrypt_authority_message($encrypted) {
  global $payswarm_jsonld_cache;

  // set defaults
  if(!isset($options['cache'])) {
    $options['cache'] = $payswarm_jsonld_cache;
  }
  if(!isset($options['check_nonce'])) {
    $options['check_nonce'] = 'payswarm_check_nonce';
  }
  if(!isset($options['check_key_owner'])) {
    $options['check_key_owner'] = 'payswarm_check_key_owner';
  }

  return bedrock_decrypt_secure_message($encrypted, $options);
}

/**
 * Gets the config for a PaySwarm Authority.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $host the PaySwarm Authority host and port.
 *
 * @return stdClass the PaySwarm Authority config.
 */
function payswarm_get_authority_config($host) {
  global $payswarm_hooks;
  global $payswarm_jsonld_cache;
  $options = array(
    'service' => 'payswarm',
    'cache' => $payswarm_jsonld_cache
  );
  if(isset($payswarm_hooks['get_jsonld'])) {
    $options['get_jsonld'] = $payswarm_hooks['get_jsonld'];
  }
  return bedrock_get_endpoint_config($host, $options);
}

/**
 * Caches a license at the PaySwarm Authority and returns the result.
 *
 * @param string $host the PaySwarm Authority host and port.
 * @param string $id the ID of the license to cache.
 *
 * @return stdClass the license.
 */
function payswarm_authority_cache_license($host, $id) {
  global $payswarm_hooks;

  // get license service
  $config = payswarm_get_authority_config($host);
  $url = $config->licenseService;

  // sign message
  $msg = new stdClass();
  $msg->{'@context'} = payswarm_get_default_jsonld_context_url();
  $msg->license = $id;
  payswarm_sign($msg, array(
    'key' => call_user_func($payswarm_hooks['get_private_key']),
    'keyId' => call_user_func($payswarm_hooks['get_public_key_id'])
  ));

  // post message
  $rval = payswarm_post_jsonld($url, $msg);

  // check license
  if(!$rval || !is_object($rval)) {
    throw new Exception('PaySwarm Exception: ' .
      'Invalid response when caching license.');
  }
  // FIXME: use error/exception vocab
  else if(is_object($rval) && property_exists($rval, 'message')) {
    throw new Exception('PaySwarm Exception: ' .
      'Error while caching license: ' .
      $rval->message . ', ' . $rval->type);
  }

  return $rval;
}

/**
 * Generates a PEM-encoded key pair and stores it by calling the
 * 'store_key_pair' hook.
 *
 * @package payswarm
 * @since 1.0
 *
 * @return array an array with the key pair as 'public_key' and 'private_key'.
 */
function payswarm_create_key_pair() {
  global $payswarm_hooks;

  // create and store key pair
  $rval = bedrock_create_key_pair();
  call_user_func(
    $payswarm_hooks['store_key_pair'],
    $rval['public_key'], $rval['private_key']);
  return $rval;
}

/**
 * Adds a trusted PaySwarm Authority. Only trusted PaySwarm Authorities can
 * be used in financial transactions.
 *
 * @param string $host the PaySwarm Authority host and port.
 */
function payswarm_add_trusted_authority($host) {
  global $payswarm_hooks;

  // get authority config
  $config = payswarm_get_authority_config($host);

  // store authority identity
  $id = $config->authorityIdentity;
  call_user_func($payswarm_hooks['store_trusted_authority'], $id);
}

/**
 * Get the PaySwarm Authority's vendor registration URL, including the
 * parameters required to register the vendor. If a key pair does not exist
 * it will be generated, otherwise the existing key pair will be used unless
 * $overwrite_key_pair is set to true.
 *
 * @param string $host the PaySwarm Authority host and port.
 * @param string $callback the callback URL for the registration result.
 * @param boolean $overwrite_key_pair true to generate a new key-pair even if
 *          there is an existing one.
 *
 * @return string the URL for registering the vendor.
 */
function payswarm_get_register_vendor_url(
  $host, $callback, $overwrite_key_pair=false) {
  global $payswarm_hooks;

  // automatically trust given payswarm authority
  payswarm_add_trusted_authority($host);

  // get register URL from authority config
  $config = payswarm_get_authority_config($host);
  $register_url = $config->vendorRegistrationService;

  // use existing public key if overwrite is not specified
  $public_key = false;
  if(!$overwrite_key_pair) {
    $public_key = call_user_func($payswarm_hooks['get_public_key']);
  }

  // no public key available (or overwriting), generate new key pair
  if($public_key === false) {
    $key_pair = payswarm_create_key_pair();
    $public_key = $key_pair['public_key'];
  }

  // add query parameters to the register URL
  $register_url = bedrock_add_query_vars(
    $register_url, array(
    'public-key' => $public_key,
    'registration-callback' => $callback,
    'response-nonce' => payswarm_create_nonce())
  );

  return $register_url;
}

/**
 * Completes the vendor registration process by verifying the response
 * from the PaySwarm Authority.
 *
 * @param string $msg the JSON-encoded encrypted registration response message.
 *
 * @return stdClass the PaySwarm Vendor Preferences.
 */
function payswarm_register_vendor($msg) {
  global $payswarm_hooks;

  // decode message
  $prefs = payswarm_decrypt_authority_message($msg);

  // check message type
  if(bedrock_jsonld_has_type($prefs, 'Error')) {
    throw new Exception('PaySwarm Registration Exception: ' .
      $msg->errorMessage);
  }
  else if(!bedrock_jsonld_has_type($prefs, 'IdentityPreferences')) {
    throw new Exception('PaySwarm Registration Exception: ' .
      'Invalid registration response from PaySwarm Authority.');
  }

  // store public key ID
  call_user_func($payswarm_hooks['store_public_key_id'], $prefs->publicKey);

  return $prefs;
}

/**
 * Get the PaySwarm Authority's purchase URL, including the parameters
 * identifying the Listing with the Asset to be purchased.
 *
 * @param string $host the PaySwarm Authority host and port.
 * @param string $listing_id the ID (IRI) for the Listing.
 * @param string $listing_hash the hash for the Listing.
 * @param string $callback the callback URL for the purchase result.
 *
 * @return string the URL for purchasing.
 */
function payswarm_get_purchase_url(
  $host, $listing_id, $listing_hash, $callback) {

  // get purchase URL from authority config
  $config = payswarm_get_authority_config($host);
  $purchase_url = $config->paymentService;

  // add query parameters to the purchase URL
  $purchase_url = bedrock_add_query_vars(
    $purchase_url, array(
    'listing' => $listing_id,
    'listing-hash' => $listing_hash,
    'callback' => $callback,
    'response-nonce' => payswarm_create_nonce())
  );

  return $purchase_url;
}

/**
 * Performs an automated purchase on behalf of a customer who has previously
 * authorized it.
 *
 * @param string $host the PaySwarm Authority host and port.
 * @param string $id the ID (IRI) of the customer.
 * @param string $listing_id the ID (IRI) for the Listing.
 * @param string $listing_hash the hash for the Listing.
 *
 * @return string the JSON-encoded encrypted purchase response message.
 */
function payswarm_purchase($host, $id, $listing_id, $listing_hash) {
  // TODO: implement
}

/**
 * Completes the purchase process by verifying the response from the PaySwarm
 * Authority and returning the receipt.
 *
 * @param string $msg the JSON-encoded encrypted purchase response message.
 *
 * @return stdClass the PaySwarm Receipt.
 */
function payswarm_get_receipt($msg) {
  // decode message
  $receipt = payswarm_decrypt_authority_message($msg);

  // check message type
  if(bedrock_jsonld_has_type($receipt, 'Error')) {
    throw new Exception('PaySwarm Purchase Exception: ' .
      $receipt->errorMessage);
  }
  else if(!bedrock_jsonld_has_type($receipt, 'Receipt')) {
    throw new Exception('PaySwarm Purchase Exception: ' .
      'Invalid purchase response from PaySwarm Authority.');
  }

  // validate receipt
  if(!property_exists($receipt, 'contract') or !is_object($receipt->contract)) {
    throw new Exception('PaySwarm Purchase Exception: ' .
      'Unknown Receipt format.');
  }

  // validate contract
  $contract = $receipt->contract;
  if(!property_exists($contract, 'assetAcquirer') or
    !property_exists($contract, 'asset') or
    !property_exists($contract, 'license')) {
    throw new Exception('PaySwarm Purchase Exception: ' .
      'Unknown Contract format.');
  }

  return $receipt;
}

/**
 * Determines whether or not the given Listing's validity period has passed.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param stdClass $listing the Listing to check.
 *
 * @return boolean true if the validity period still applies, false if not.
 */
function payswarm_is_listing_valid($listing) {
  if(!$listing) {
    return false;
  }
  $now = date_create();
  $valid_from = date_create($listing->validFrom);
  $valid_until = date_create($listing->validUntil);
  return ($now >= $valid_from && $now <= $valid_until);
}

// cache for jsonld docs
$payswarm_jsonld_cache = (object)array(
  'get' => function($url) {
    global $payswarm_hooks;
    if(isset($payswarm_hooks['get_cached_jsonld'])) {
      return call_user_func($payswarm_hooks['get_cached_jsonld'], $url);
    }
    return false;
  },
  'set' => function($url, $response) {
    global $payswarm_hooks;
    if(isset($payswarm_hooks['cache_jsonld'])) {
      call_user_func($payswarm_hooks['cache_jsonld'], $id, $obj, 60);
    }
  }
);

/* end of file, omit ?> */
