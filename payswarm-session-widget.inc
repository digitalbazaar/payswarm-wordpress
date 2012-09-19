<?php

require_once('payswarm-session.inc');

/**
 * Defines the PaySwarm Session widget.
 *
 * @package payswarm
 * @since 1.0
 */
class PaySwarmSessionWidget extends WP_Widget {
  function PaySwarmSessionWidget() {
    parent::WP_Widget('PaySwarm_Session', 'PaySwarm Session', array(
      'classname' => 'PaySwarmSessionWidget',
      'description' => 'Indicates whether a PaySwarm session is active.'
    ));
  }

  /**
   * Renders the widget to the screen.
   *
   * @param assoc $args the widget arguments.
   * @param WP_Widget $instance the widget instance.
   */
  function widget($args, $instance) {
    extract($args);
    echo $before_widget;

    $session = payswarm_get_session();
    if($session && $session['identity_id'] !== null) {
      $url = plugins_url() . '/payswarm/payswarm-logout.php?redirect=' .
        urlencode($_SERVER['REQUEST_URI']);
      $pslogo_url = PAYSWARM_PLUGIN_URL . 'images/payswarm-20.png';
      echo "<span class=\"payswarm-session\" title=\"Logged into PaySwarm as '{$session['identity_id']}'\"><img src=\"$pslogo_url\" />Logged into PaySwarm <span class=\"right\">(<a href=\"$url\">Logout</a>)</span></span>";
    }/*
    else {
      echo "<span class=\"payswarm-session\">Not logged into PaySwarm</span>";
    }*/

    echo $after_widget;
  }

  /**
   * Called when a user clicks 'Save' on the settings page on the widget
   * administration screen.
   *
   * @param WP_Widget $new_instance the new widget instance.
   * @param WP_Widget $old_instance the old widget instance.
   *
   * @return WP_Widget the new widget instance.
   */
  function update($new_instance, $old_instance) {
    return $new_instance;
  }

  /**
   * Displays the settings page for the widget on the widget administration
   * screen.
   *
   * @param WP_Widget $instance the widget instance.
   */
  /*function form($instance) {
  }*/
}

/**
 * Installs default PaySwarm widgets on plugin activation.
 */
function payswarm_install_widgets() {
  // FIXME: implement me
  $fp = fopen('/tmp/php.txt', 'a');
  fwrite($fp, "payswarm plugin activated\n");
  fclose($fp);
}

/* end of file, omit ?> */
