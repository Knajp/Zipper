#!/bin/bash

mkdir ./shader/bin
$VULKANSDK/x86_64/bin/glslc shader/shader.vert -o shader/bin/vert.spv
$VULKANSDK/x86_64/bin/glslc shader/shader.frag -o shader/bin/frag.spv
$VULKANSDK/x86_64/bin/glslc shader/text.vert -o shader/bin/textvert.spv
$VULKANSDK/x86_64/bin/glslc shader/text.frag -o shader/bin/textfrag.spv
$VULKANSDK/x86_64/bin/glslc shader/scene.vert -o shader/bin/scenevert.spv
$VULKANSDK/x86_64/bin/glslc shader/scene.frag -o shader/bin/scenefrag.spv