#!/bin/sh
rm -rf build && mkdir build
cd build
cmake -DSCAP_WORKBENCH_SCAP_CONTENT_DIRECTORY:PATH=/usr/local/share/xml/scap -DSCAP_WORKBENCH_SSG_DIRECTORY:FILEPATH=/usr/local/share/xml/scap/ssg/content ..
