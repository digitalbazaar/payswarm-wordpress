<?php

global $wpdb;

/**
 * The current database schema version.
 * @since 1.0
 * @global string PAYSWARM_SCHEMA_VERSION
 */
define('PAYSWARM_SCHEMA_VERSION', '1.0');

/**
 * The name of the table where PaySwarm stores identity IDs.
 * @since 1.0
 * @global string PAYSWARM_IDENTITY_TABLE_NAME
 */
define('PAYSWARM_IDENTITY_TABLE_NAME',
  $wpdb->prefix . 'payswarm_identity');

/**
 * The name of the table where PaySwarm stores which posts have been authorized.
 * @since 1.0
 * @global string PAYSWARM_AUTHORIZED_POSTS_TABLE_NAME
 */
define('PAYSWARM_AUTHORIZED_POSTS_TABLE_NAME',
  $wpdb->prefix . 'payswarm_authorized_posts');

/**
 * Installs/upgrades the PaySwarm database into a WordPress installation.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_install_database() {
  global $wpdb;

  // check to see if the payswarm identity table exists
  if($wpdb->get_var(
    'SHOW TABLES LIKE \''. PAYSWARM_IDENTITY_TABLE_NAME .'\'') !==
    PAYSWARM_IDENTITY_TABLE_NAME) {
    // ensure that the formatting below is one field per line, dbDelta is
    // very picky when it comes to what it allows.
    $posts_sql = 'CREATE TABLE ' . PAYSWARM_IDENTITY_TABLE_NAME . ' (
      identity_key INTEGER UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
      identity_hash VARCHAR(36) NOT NULL,
      identity_id TEXT NOT NULL,
      UNIQUE(identity_hash));';

    require_once(ABSPATH . 'wp-admin/includes/upgrade.php');
    dbDelta($posts_sql);
    update_option("payswarm_schema_version", PAYSWARM_SCHEMA_VERSION);
  }

  // check to see if the payswarm authorized posts table exists
  if($wpdb->get_var(
    'SHOW TABLES LIKE \''. PAYSWARM_AUTHORIZED_POSTS_TABLE_NAME .'\'') !==
    PAYSWARM_AUTHORIZED_POSTS_TABLE_NAME) {
    // ensure that the formatting below is one field per line, dbDelta is
    // very picky when it comes to what it allows.
    $posts_sql = 'CREATE TABLE ' . PAYSWARM_AUTHORIZED_POSTS_TABLE_NAME . ' (
      identity_key INTEGER UNSIGNED NOT NULL,
      post_id INTEGER NOT NULL,
      PRIMARY KEY(identity_key,post_id)
      );';

    require_once(ABSPATH . 'wp-admin/includes/upgrade.php');
    dbDelta($posts_sql);
    update_option("payswarm_schema_version", PAYSWARM_SCHEMA_VERSION);
  }

  // NOTE: We need to put in code here to upgrade the schema if we ever change
  // the database schema
}

/**
 * Uninstalls the PaySwarm database from a WordPress installation.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_uninstall_database() {
  // FIXME: remove payswarm tables? this would cause articles to lose their
  // prices, etc. ... perhaps we want the code to use default prices if they
  // haven't been set?
}

/**
 * Generates a identity hash.
 *
 * @param string $identity_id the ID of a identity.
 *
 * @return string the identity hash.
 */
function payswarm_database_hash_identity_id($identity_id) {
  // do md5 hash + length of identity
  return md5($identity_id) . sprintf('%04x', strlen($identity_id));
}

/**
 * Authorizes a post to be viewed for a particular identity. If the given
 * contract is invalid, the post will not be authorized.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $identity_id the ID of the user on the site.
 * @param Integer $post_id the post identifier.
 * @param string $license the URL of the license that was authorized.
 *
 * @return boolean true if the post was authorized, false if not.
 */
function payswarm_database_authorize_post($identity_id, $post_id, $license) {
  $rval = false;
  global $wpdb;

  // TODO: add support for different authorizations based on license
  // FIXME: check post meta data against license URL for now?

  // insert identity ID
  $identity_hash = payswarm_database_hash_identity_id($identity_id);
  $rval = $wpdb->query($wpdb->prepare(
    'INSERT IGNORE INTO ' . PAYSWARM_IDENTITY_TABLE_NAME . ' ' .
    '(identity_hash, identity_id) VALUES (%s,%s)',
    $identity_hash, $identity_id));

  if($rval !== false) {
    // insert authorized post entry
    // important: 'false' check is necessary because the query can return 0
    $rval = ($wpdb->query($wpdb->prepare(
       'INSERT IGNORE INTO ' . PAYSWARM_AUTHORIZED_POSTS_TABLE_NAME . ' ' .
       '(identity_key,post_id) VALUES (' .
       '(SELECT identity_key FROM ' . PAYSWARM_IDENTITY_TABLE_NAME . ' ' .
       ' WHERE identity_hash=%s AND identity_id=%s),%s)',
       $identity_hash, $identity_id, $post_id)) !== false);
  }

  return $rval;
}

/**
 * Checks to see if a post is authorized using the given session. A post is
 * authorized if the identity ID in the session has been granted access to
 * view the post.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param Array $session the session for the user on the site.
 * @param Integer $post_id the post identifier.
 *
 * @return boolean true if the post is authorized, false if not.
 */
function payswarm_database_is_post_authorized($session, $post_id) {
  $rval = false;
  global $wpdb;

  if($session !== false and $session['identity_id'] !== null) {
    $identity_hash = payswarm_database_hash_identity_id(
      $session['identity_id']);
    $rval = $wpdb->get_row($wpdb->prepare(
      'SELECT ap.post_id as post_id FROM ' .
      PAYSWARM_AUTHORIZED_POSTS_TABLE_NAME . ' ap ' .
      ' JOIN ' . PAYSWARM_IDENTITY_TABLE_NAME . ' p ' .
      ' ON ap.identity_key=p.identity_key' .
      ' WHERE p.identity_hash=%s AND' .
      ' p.identity_id=%s AND ap.post_id=%s LIMIT 1',
      $identity_hash, $session['identity_id'], $post_id), ARRAY_A);
    $rval = (is_array($rval) and count($rval) === 1);
  }

  return $rval;
}

/**
 * Deletes old post metadata from a post given a post ID and metadata ID and
 * name.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param Integer $post_id the post identifier.
 * @param Integer $meta_id the metadata identifier.
 * @param String $meta_key the metadata key value.
 */
function payswarm_database_remove_old_metadata($post_id, $meta_id, $meta_key) {
  global $wpdb;

  // delete the post identifier
  $wpdb->query($wpdb->prepare(
    'DELETE FROM ' . $wpdb->postmeta . ' ' .
    'WHERE post_id=%s AND meta_id<%s AND meta_key=%s',
    $post_id, $meta_id, $meta_key));
}

/* end of file, omit ?> */
