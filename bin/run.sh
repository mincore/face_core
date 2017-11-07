#gpu
export GPU_IDS=0,1

#alg 
export ALG_EXTRACT_BATCHS=20,15,10,5,1
export ALG_DETECT_THREAD_COUNT=10
export ALG_COMPARE_THREAD_COUNT=20

#MODEL
export ALG_MODEL="./model"

export ALG_DETECTMODEL=""
export ALG_DETECTTRAINED=""
export ALG_FEATUREMODEL="sphereface_ave_norm.prototxt"
export ALG_FEATURETRAINED="sphereface_sphereface_size112x96_30-52_66-52_48-92_28000.caffemodel"
export ALG_QUALITYMODEL="qa_deploy.prototxt"
export ALG_QUALITYTRAINED="qa_iter_50000.caffemodel"
export ALG_PROPERTYMODEL="prop_deploy.prototxt"
export ALG_PROPERTYTRAINED="prop_iter_50000.caffemodel"

#debug
export CAPTURE_PATH=

./test $@
