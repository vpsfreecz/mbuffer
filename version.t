#ifndef VERSION_H
#define VERSION_H

#define VERSION		\"{latesttag}{if(latesttagdistance,'.{latesttagdistance}')}$DELTA (hg:{rev}/{node|short})\"
#define HG_REV		\"{rev}\"
#define HG_BRANCH	\"{branch}\"
#define HG_NODE		\"{node}\"
#define HG_ID		\"{node|short}\"
#define HG_TAGS		\"{tags}\"
#define HG_LATESTTAG	\"{latesttag}\"\n

#endif\n
