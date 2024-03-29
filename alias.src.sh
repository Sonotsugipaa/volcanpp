prof=${prof:-debug}

function mk {
	clear && ./build.sh $prof; }

function mk-run {
	clear && ./build.sh $prof && (cd $prof && ./launch.sh); }

function mk-grind {
	clear && ./build.sh $prof && (cd $prof && ./launch.sh -d); }

function mk-grindleaks {
	clear && ./build.sh $prof && (cd $prof && ./launch.sh -l); }

function rm-mk {
	clear && ./build.sh -r $prof; }

function rm-mk-run {
	clear && ./build.sh -r $prof && (cd $prof && ./launch.sh); }

function rm-mk-grind {
	clear && ./build.sh -r $prof && (cd $prof && ./launch.sh -d); }

function rm-mk-grindleaks {
	clear && ./build.sh -r $prof && (cd $prof && ./launch.sh -l); }


function git-tarxz {
	local dirname="$(basename "$(pwd)")"
	cd .git
	tar --xz -cf ../"$dirname".git.tar.xz ./*
	cd ..
	echo "Created '$dirname.git.tar.xz'"
}
