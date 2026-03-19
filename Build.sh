#!/bin/bash

isDebugBuild=true
cleanBuild=false
build=true
runAfterBuild=false
directoryName=""
gameName="GoknarEngineEditor"

for argument in "$@"
do
    if [[ "$argument" == "nobuild" ]]; then
    	build=false
    elif [[ "$argument" == "release" ]]; then
    	isDebugBuild=false
    elif [[ "$argument" == "run" ]]; then
	    runAfterBuild=true
    elif [[ "$argument" == "clean" ]]; then
		cleanBuild=true
	fi
done

sync_directories() {
    local dirs=("Content" "EditorContent" "Config")
    
    for dir in "${dirs[@]}"; do
        if [ -d "../$dir" ]; then
            # Find all files in the source directory recursively
            find "../$dir" -type f | while read -r src_file; do
                
                # Determine the matching destination path
                # This strips the "../" prefix from the source path to build the dest path
                dst_file="./${src_file#../}"
                
                # 1. If the file doesn't exist in the destination folder at all
                if [ ! -f "$dst_file" ]; then
                    mkdir -p "$(dirname "$dst_file")"
                    cp "$src_file" "$dst_file"
                else
                    # 2. If it does exist, calculate and compare the MD5 sums
                    # (Note: macOS uses 'md5 -q' instead of 'md5sum | awk')
                    src_md5=$(md5sum "$src_file" | awk '{print $1}')
                    dst_md5=$(md5sum "$dst_file" | awk '{print $1}')
                    
                    if [ "$src_md5" != "$dst_md5" ]; then
                        echo "Changes detected in $src_file. Updating..."
                        cp "$src_file" "$dst_file"
                    fi
                fi
            done
        fi
    done
}

if [ "$isDebugBuild" = true ]; then
	directoryName="Build_Debug"
	rm -f $directoryName/$gameName
	
	if [ "$cleanBuild" = true ]; then
		rm -rf $directoryName
	fi
	
	mkdir -p $directoryName && cd $directoryName
	sync_directories
	if [ "$build" = true ]; then
		cmake -DCMAKE_BUILD_TYPE=Debug .. && make -j 8
	fi
else
	directoryName="Build_Release"
	rm -f $directoryName/$gameName
	
	if [ "$cleanBuild" = true ]; then
		rm -rf $directoryName
	fi
	
	mkdir -p $directoryName && cd $directoryName
	sync_directories
	if [ "$build" = true ]; then
		cmake -DCMAKE_BUILD_TYPE=Release .. && make -j 8
	fi
fi

if [ "$runAfterBuild" = true ]; then
	./$gameName
fi