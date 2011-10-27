<?php

require_once('jsonld.php');

/**
 * Creates a default payswarm JSON-LD context.
 *
 * @return the default payswarm JSON-LD context.
 */
function payswarm_create_default_jsonld_context()
{
   $ctx = (object)array(
      'com' => 'http://purl.org/commerce#',
      'dc' => 'http://purl.org/dc/terms/',
      'foaf' => 'http://xmlns.com/foaf/0.1/',
      'gr' => 'http://purl.org/goodrelations/v1#',
      'ps' => 'http://purl.org/payswarm#',
      'psp' => 'http://purl.org/payswarm/preferences#',
      'rdf' => 'http://www.w3.org/1999/02/22-rdf-syntax-ns#',
      'rdfs' => 'http://www.w3.org/2000/01/rdf-schema#',
      'sec' => 'http://purl.org/security#',
      'vcard' => 'http://www.w3.org/2006/vcard/ns#',
      'xsd' => 'http://www.w3.org/2001/XMLSchema#',
      '@coerce' => (object)array(
         'com:destination' => '@iri',
         'com:destinationOwnerType' => '@iri',
         'com:payee' => '@iri',
         'com:rateContext' => '@iri',
         'com:rateType' => '@iri',
         'com:source' => '@iri',
         'dc:creator' => '@iri',
         'ps:asset' => '@iri',
         'ps:assetProvider' => '@iri',
         'ps:authority' => '@iri',
         'ps:contentUrl' => '@iri',
         'ps:license' => '@iri',
         'sec:signer' => '@iri',
         'com:payeePosition' => 'xsd:integer',
         'dc:created' => 'xsd:dateTime',
         'com:date' => 'xsd:dateTime',
         'ps:validFrom' => 'xsd:dateTime',
         'ps:validUntil' => 'xsd:dateTime',
         'ps:licenseTemplate' => 'rdf:XMLLiteral')
   );

   return $ctx;
}

/**
 * JSON-encodes (with unescaped slashes) the given data.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $data The native PHP data which will be converted to JSON by
 *    json_encode().
 *
 * @return The encoded JSON data.
 */
function payswarm_json_encode($data)
{
   // Newer PHP has a flag to avoid escaped '/'
   if(defined('JSON_UNESCAPED_SLASHES'))
   {
      $json = json_encode($data, JSON_UNESCAPED_SLASHES);
   }
   else
   {
      // Use a simple string replacement of '\/' to '/'.
      // FIXME: does this need to be more complex?
      $json = str_replace('\\/', '/', json_encode($data));
   }

   return $json;
}

/**
 * Decodes a serialized JSON-LD object and resolves any @context URLs.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string @input the JSON-LD input.
 *
 * @return mixed the resolved JSON-LD object, null on error.
 */
function payswarm_jsonld_decode($input)
{
   // JSON-decode
   $rval = json_decode($input);
   if($rval !== null)
   {
      // resolve any @context URLs
      // FIXME: uncomment once @context URLs are defined
      /*
      $rval = jsonld_resolve($rval, function($url)
      {
         // get @context URL from cache
         $id = 'payswarm_jsonld_ctx_' . md5($url);
         $ctx = get_transient($id);
         if($ctx === false)
         {
            $response = wp_remote_get($url, array(
               'sslverify' => PAYSWARM_SSL_ENABLED,
               'headers' => array('Accept' => 'application/ld+json')));
            if(is_wp_error($response))
            {
               throw new Exception("Could not get @context from '$url': " .
                  $response>get_error_message());
            }

            // JSON-decode and update cache
            $ctx = json_decode($response['body']);
            if($ctx === null)
            {
               throw new Exception("Could not get $context from '$url': " .
                  'Malformed JSON.');
            }
            set_transient($id, $ctx, 60*60*24);
         }
         return $ctx;
      });*/
   }

   return $rval;
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
   $response = payswarm_jsonld_decode($e->lastResponse);

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
