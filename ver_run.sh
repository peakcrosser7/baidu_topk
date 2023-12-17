# run v0~v10 topk code

# ROOT_DIR=$(cd $(dirname $0); pwd)
VERSION=$1
ROOT_DIR=$(cd $(dirname $0); pwd)
query_dir=$2
doc_file=$3
output_file=$4

if [ -z "$VERSION" ]; then
    topk_file="query_doc_scoring"
else
    topk_file="query_doc_scoring_"$VERSION
fi

topk_path=$ROOT_DIR"/bin/"$topk_file
if [ ! -f "$topk_path" ]; then
    echo "${topk_path} does not exist"
    exit 1
fi

echo "current topk file: ${topk_file}"
sleep 2s

if [ -z "$doc_file" ]; then
        doc_file=./translate/docs.txt
fi

if [ -z "$output_file" ]; then
        output_file=./translate/res/result_${VERSION}.txt
fi

if [ -z "$query_dir" ]; then
        query_dir=./translate/querys2000
fi

$topk_path ${doc_file} ${query_dir} ${output_file}
echo "run success"