This directory contains metadata for F-Droid.

For details on the formats, see <https://f-droid.org/docs/All_About_Descriptions_Graphics_and_Screenshots/>

Note that each released version ends up with four versions on F-Droid, one for each ABI. The changelog files are duplicated accordingly.

The fdroid-version.txt needs to be updated on releases that are supposed to get pushed to F-Droid to contain the version name and code to use there, the latter without the ABI digit. The algorithm for the version code calculation is found in [cmake/DrawpileVersions.cmake](../cmake/DrawpileVersions.cmake).
