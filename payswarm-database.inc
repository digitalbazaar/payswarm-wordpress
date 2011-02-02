<?php
global $wpdb;
/**
 * The current database schema version.
 * @since 1.0
 * @global string PAYSWARM_SCHEMA_VERSION
 */
define('PAYSWARM_SCHEMA_VERSION', '1.0');

/**
 * The name of the table where PaySwarm stores session/token information.
 * @since 1.0
 * @global string PAYSWARM_TABLE_NAME
 */
define('PAYSWARM_TABLE_NAME', $wpdb->prefix . 'payswarm_tokens');

/**
 * Installs/upgrades the PaySwarm database into a WordPress installation.
 * 
 * @package payswarm
 * @since 1.0
 */
function payswarm_install_database()
{
   global $wpdb;

   // check to see if the payswarm table exists
   if($wpdb->get_var('SHOW TABLES LIKE \''. PAYSWARM_TABLE_NAME .'\'') != 
      PAYSWARM_TABLE_NAME)
   {
      // ensure that the formatting below is one field per line, dbDelta is
      // very picky when it comes to what it allows.
      $sql = 'CREATE TABLE ' . PAYSWARM_TABLE_NAME . ' (
         session VARCHAR(255) NOT NULL,
         scope TINYTEXT,
         state TINYTEXT,
         token TINYTEXT,
         secret TINYTEXT,
         details TEXT,
         mtime TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
         PRIMARY KEY (session)
         );';

      require_once(ABSPATH . 'wp-admin/includes/upgrade.php');
      dbDelta($sql);
      update_option("payswarm_schema_version", PAYSWARM_SCHEMA_VERSION);
   }

   // NOTE: We need to put in code here to upgrade the schema if we ever change
   // the database schema
}

/**
 * Retrieves the token associated with a PaySwarm session.
 * 
 * Retrieves all of the token information that is associated with a PaySwarm
 * session. The token contains information such as session, state, the token
 * identifier, token secret, and the amount left on the token.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $payswarm_session the PaySwarm session identifier.
 * @param string $scope the intended scope of the PaySwarm session identifier.
 * @param string $create true if the token should be created
 * @return Array An associative array of all items associated with the given
 *    PaySwarm session or NULL if no session exists.
 */
function payswarm_database_get_token(
   $payswarm_session, $scope, $create=false)
{
   $rval = null;
   global $wpdb;

   // create the token if it doesn't already exist in the database
   if($create)
   {
      $wpdb->query($wpdb->prepare('INSERT IGNORE INTO ' . PAYSWARM_TABLE_NAME . 
         '(session, scope, state, token, secret, details) ' .
         'VALUES (%s, %s, %s, %s, %s, %s)',
         $payswarm_session, $scope, 'initializing', '', '', 
         '{"balance": "0.0", "authorized_posts": ""}'));
   }

   // retrieve all information associated with the session
   $rval = $wpdb->get_row($wpdb->prepare(
      'SELECT session, scope, state, token, secret, details FROM ' . 
      PAYSWARM_TABLE_NAME . ' WHERE session=%s AND scope=%s LIMIT 1', 
      $payswarm_session, $scope), ARRAY_A);
   
   // set the return type to null if there was nothing in the database
   if($wpdb->num_rows < 1)
   {
      $rval = null;
   }
   else
   {
      // decode details
      $rval['details'] = json_decode($rval['details'], true);
   }

   return $rval;
}

/**
 * Updates a PaySwarm Token given information associated with the token.
 * 
 * Updates all of the token information that is provided. The token contains 
 * information such as session, state, the token identifier, token secret, 
 * and the amount left on the token.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param Array $payswarm_token An associative array containing all of the 
 *    values for a $payswarm_token that should be updated. 
 * @return boolean true if the update was successful, false otherwise.
 */
function payswarm_database_update_token($payswarm_token)
{
   $rval = false;
   global $wpdb;

   // encode details
   $payswarm_token['details'] = json_encode($payswarm_token['details']);
      
   // build the update data in WordPress database update format
   $data = array();
   foreach($payswarm_token as $key => $value)
   {
      $data[$key] = $value;
   }

   // update all information associated with the session
   $wpdb->update(PAYSWARM_TABLE_NAME, $data, 
      array('session' => $payswarm_token['session'], 
         'scope' => $payswarm_token['scope']));

   // Set the return type to true if the object was updated correctly
   if($wpdb->num_rows > 0)
   {
      $rval = true;
   }

   return $rval;
}

/**
 * Deletes a PaySwarm Token given information associated with the token.
 * 
 * The information given must at least contain the session and scope associated
 * with the token.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param Array $payswarm_token An associative array containing at least 
 *    the session and scope for the token that should be deleted. 
 * @return boolean true if the delete was successful, false otherwise.
 */
function payswarm_database_delete_token($payswarm_token)
{
   $rval = false;
   global $wpdb;

   // delete all information associated with the session
   $dstatement = "DELETE FROM " . PAYSWARM_TABLE_NAME . 
      " WHERE session='%s' AND scope = '%s'";
   $wpdb->query($wpdb->prepare($dstatement, 
        $payswarm_token['session'], $payswarm_token['scope']));
   
   // Set the return type to true if one or more objects were deleted
   if($wpdb->num_rows > 0)
   {
      $rval = true;
   }

   return $rval;
}

?>
