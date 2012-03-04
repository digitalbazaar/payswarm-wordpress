/*
 * PaySwarm JavaScript library.
 *
 * Copyright 2010-2012 Digital Bazaar, Inc.
 * License: LGPLv3.
 */

/**
 * Expands the asset information when the title is clicked.
 *
 * @param postId the identifier for the post
 */
function toggleAssetInformation(postId) {
  var assetInfo = document.getElementById('payswarm-asset-info-' + postId);
  var className = assetInfo.getAttribute('class');

  // toggle the hidden CSS state for the attribute
  if(className === 'hidden') {
    assetInfo.setAttribute('class', '');
  }
  else {
    assetInfo.setAttribute('class', 'hidden');
  }
}

/**
 * Sets the target of a form submission to a popup that will redirect to
 * the PaySwarm Authority.
 *
 * @param form the form that is being submitted.
 */
function showAuthorityPopup(form) {
  var width = 700;
  var height = 600;
  var popup = window.open('', 'payswarm',
    'left=' + ((screen.width-width)/2) +
    ',top=' + ((screen.height-height)/2) +
    ',width=' + width +
    ',height=' + height +
    ',resizeable,scrollbars');
  form.target = 'payswarm';
}

/**
 * Closes any pop up window and loads the given url.
 *
 * @param url the URL to load in the parent.
 */
function closePopup(url) {
  if(window.opener === null) {
    window.location = url;
  }
  else {
    window.close();
    window.opener.location = url;
  }
}
