<?php

// FIXME: needs internationalization
// $vars is provided externally
$vars['output'] = <<<RDFA
<div
  {$vars['prefixes']}
  about="{$vars['asset']('@id')}"
  typeof="{$vars['asset']('@type')}"
  class="asset-info">
  <div about="{$vars['listing']('@id')}" typeof="{$vars['listing']('@type')}">
    <form action="{$vars['access_url']}" method="post"
      onsubmit="javascript:showAuthorityPopup(this);">
      <button class="purchase-button">
        <img alt="{$vars['text_access']}" src="{$vars['pslogo_url']}" />
        {$vars['text_access']}
      </button>
      <span rel="ps:asset" resource="{$vars['asset']('@id')}">
        {$vars['text_view_full_article']}
      </span>
      <span rel="com:payee" resource="{$vars['payee']('@id')}">
        <span>$</span>
        <span rel="com:currency"
          resource="{$vars['payee']('com:currency')}"></span>
        <abbr property="com:payeeRate" title="{$vars['payee']('com:payeeRate')}"
          content="{$vars['payee']('com:payeeRate')}">{$vars['payee_rate']}
        </abbr>
        </span>
      <span class="view-more"
        onclick="javascript:toggleAssetInformation($post_id);">
        {$vars['text_view_offer_details']}
      </span>
    </form>
  </div>
  <div id="payswarm-asset-info-$post_id" class="hidden">
    <a rel="ps:assetContent" href="{$vars['asset']('ps:assetContent')}"
      property="dc:title">{$vars['asset']('dc:title')}</a> by
    <span rel="dc:creator">
      <span property="vcard:fn">{$vars['asset']('dc:creator', 'vcard:fn')}</span>
    </span>.
    Written by
    <a rel="ps:assetProvider"
      href="{$vars['asset']('ps:assetProvider')}">us</a>.
    Offered by
    <span rel="ps:listingRestrictions">
      <a rel="com:vendor"
        href="{$vars['asset']('ps:listingRestrictions', 'com:vendor')}">us</a>.
    </span>
    <span about="{$vars['listing']('@id')}">The offer to get access to this
      <span property="ps:assetHash"
        content="{$vars['listing']('ps:assetHash')}">post</span>
      under <a rel="ps:license" href="{$vars['listing']('ps:license')}"
        property="ps:licenseHash"
        content="{$vars['listing']('ps:licenseHash')}">this license</a>
      is valid from <abbr property="ps:validFrom"
        title="{$vars['listing']('ps:validFrom')}"
        content="{$vars['listing']('ps:validFrom')}"
        datatype="{$vars['listing']('ps:validFrom', '@type')}">today</abbr>
      until <abbr property="ps:validUntil"
         title="{$vars['listing']('ps:validUntil')}"
         content="{$vars['listing']('ps:validUntil')}"
         datatype="{$vars['listing']('ps:validUntil', '@type')}">tomorrow</abbr>.
    </span>
    <span rel="sec:signature">
      <span typeof="{$vars['asset']('sec:signature', '@type')}">
        This <a href="{$vars['asset']('@id')}">asset</a> was
        <abbr property="sec:signatureValue"
          content="{$vars['asset']('sec:signature', 'sec:signatureValue')}"
          title="{$vars['asset']('sec:signature', 'sec:signatureValue')}">
          digitally signed</abbr>
        using <a rel="dc:creator"
          href="{$vars['asset']('sec:signature', 'dc:creator')}">
          this key</a>
        at <span property="dc:created"
          datatype="{$vars['asset']('sec:signature', 'dc:created', '@type')}">{$vars['asset']('sec:signature', 'dc:created')}</span>.
      </span>
    </span>
    <span about="{$vars['listing']('@id')}">
      <span rel="com:vendor" resource="{$vars['listing']('com:vendor')}"></span>
      <span rel="com:payeeRule">
        <span typeof="{$vars['payee_rule']('@type')}">
          <span property="com:payeeGroupPrefix" content="{$vars['payee_rule']('com:payeeGroupPrefix')}"></span>
          <span rel="com:payeeRateType" resource="{$vars['payee_rule']('com:payeeRateType')}"></span>
          <span rel="com:payeeApplyType" resource="{$vars['payee_rule']('com:payeeApplyType')}"></span>
          <span property="com:maximumPayeeRate"
            content="{$vars['payee_rule']('com:maximumPayeeRate')}"></span>
        </span>
      </span>
      <span rel="sec:signature">
        <span typeof="{$vars['listing']('sec:signature', '@type')}">
          This <a href="{$vars['listing']('@id')}">listing</a> was
          <abbr property="sec:signatureValue"
            content="{$vars['listing']('sec:signature', 'sec:signatureValue')}"
            title="{$vars['listing']('sec:signature', 'sec:signatureValue')}">
            digitally signed</abbr>
          using <a rel="dc:creator"
            href="{$vars['listing']('sec:signature', 'dc:creator')}">
            this key</a>
          at <span property="dc:created"
            datatype="{$vars['listing']('sec:signature', 'dc:created', '@type')}">{$vars['listing']('sec:signature', 'dc:created')}</span>.
        </span>
      </span>
    </span>
    <span about="{$vars['payee']('@id')}"
      typeof="{$vars['payee']('@type')}">
      <span property="com:payeeGroup" content="{$vars['payee']('com:payeeGroup')}">
      </span>
      <span rel="com:payeeApplyType" resource="{$vars['payee']('com:payeeApplyType')}">
      </span>
      <span rel="com:payeeRateType" resource="{$vars['payee']('com:payeeRateType')}">
        Flat-rate payment</span> will be deposited into
      <a rel="com:destination"
        href="{$vars['payee']('com:destination')}">this account</a>.
      The memo field for the payment will read:
        &quot;
        <span property="rdfs:comment">{$vars['payee']('rdfs:comment')}</span>
        &quot;
    </span>
  </div>
</div>
RDFA;

/* end of file, omit ?> */
