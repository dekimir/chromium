// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/flash_embed_rewrite.h"

#include "base/metrics/histogram_macros.h"
#include "url/gurl.h"

namespace {

void RecordYouTubeRewriteUMA(FlashEmbedRewrite::YouTubeRewriteStatus status) {
  UMA_HISTOGRAM_ENUMERATION(FlashEmbedRewrite::kFlashYouTubeRewriteUMA, status,
                            FlashEmbedRewrite::YouTubeRewriteStatus::kLast);
}

}  // namespace

const char FlashEmbedRewrite::kFlashYouTubeRewriteUMA[] =
    "Plugin.Flash.YouTubeRewrite";

GURL FlashEmbedRewrite::RewriteFlashEmbedURL(const GURL& url) {
  DCHECK(url.is_valid());

  if (url.DomainIs("youtube.com") || url.DomainIs("youtube-nocookie.com"))
    return RewriteYouTubeFlashEmbedURL(url);

  return GURL();
}

GURL FlashEmbedRewrite::RewriteYouTubeFlashEmbedURL(const GURL& url) {
  // YouTube URLs are of the form of youtube.com/v/VIDEO_ID. So, we check to see
  // if the given URL does follow that format.
  if (url.path().find("/v/") != 0)
    return GURL();

  std::string url_str = url.spec();
  YouTubeRewriteStatus result = YouTubeRewriteStatus::kLast;

  // If the website is using an invalid YouTube URL, we'll try and
  // fix the URL by ensuring that if there are multiple parameters,
  // the parameter string begins with a "?" and then follows with a "&"
  // for each subsequent parameter. We do this because the Flash video player
  // has some URL correction capabilities so we don't want this move to HTML5
  // to break webpages that used to work.
  size_t index = url_str.find_first_of("&?");
  bool invalid_url = index != std::string::npos && url_str.at(index) == '&';

  if (invalid_url) {
    // ? should appear first before all parameters
    url_str.replace(index, 1, "?");

    // Replace all instances of ? (after the first) with &
    for (size_t pos = index + 1;
         (pos = url_str.find("?", pos)) != std::string::npos; pos += 1) {
      url_str.replace(pos, 1, "&");
    }
  }

  GURL corrected_url = GURL(url_str);
  // Chrome used to only rewrite embeds with enablejsapi=1 on mobile for
  // backward compatibility but with Flash embeds deprecated by YouTube, they
  // are rewritten on all platforms. However, a different result is used in
  // order to keep track of how popular they are.
  if (corrected_url.query().find("enablejsapi=1") != std::string::npos)
    result = YouTubeRewriteStatus::kSuccessEnableJSAPI;

  // Change the path to use the YouTube HTML5 API
  std::string path = corrected_url.path();
  path.replace(path.find("/v/"), 3, "/embed/");

  url::Replacements<char> r;
  r.SetPath(path.c_str(), url::Component(0, path.length()));

  if (result == YouTubeRewriteStatus::kLast) {
    result = invalid_url ? YouTubeRewriteStatus::kSuccessParamsRewrite
                         : YouTubeRewriteStatus::kSuccess;
  }

  RecordYouTubeRewriteUMA(result);
  return corrected_url.ReplaceComponents(r);
}
