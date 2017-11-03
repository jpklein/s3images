# Drupal 7 S3 Image File Handling

## Problem
The s3fs contrib module was saving a copy of files to the local db in order to display thumbnail previews, which placed an unnecessary burden on our persistence and caching layers and essentially nullified the benefits of hosting assets in s3.

## Solution
Using hooks and class overrides, I was able to exploit quirks in the way that drupal processes asset uploads and renders images on entity forms to rely solely on remote copies of image files.

(NB this sample is a subset of code from a module containing several functions shared across a constellation of Drupal installs and may contain references to modules & functions outside of this repository)
