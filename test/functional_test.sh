#!/bin/bash 

echo -----------------------
echo httpsrv functional test
echo -----------------------

host_and_port="localhost:8080"

# ------------------------------------------------------------------------------
# Create a temporary dir and initializes some file vars
# ------------------------------------------------------------------------------

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

tmp_dir=$(mktemp -d -t resources-XXXXXXXXXX)
tmp_dir2=$(mktemp -d -t resources-XXXXXXXXXX)

if [ -d "$tmp_dir" ]; then
  echo "Created ${tmp_dir}..."
else
  echo "Error: ${tmp_dir} not found. Can not continue."
  exit 1
fi

listfile=$tmp_dir"/list.tmp"
resultfile=$tmp_dir"/result.tmp"
allidsfile=$tmp_dir"/allids.tmp"
mruidsfile=$tmp_dir"/mruids.tmp"
threeidsfile=$tmp_dir"/threeids.tmp"
sortedmruidsfile=$tmp_dir"/sortedmruids.tmp"

echo "Creating 10 files in $tmp_dir ..."
for i in {01..10}
do
    echo hello > "$tmp_dir/File${i}.txt"
done

rm -f $resultfile

# ------------------------------------------------------------------------------
# Uploading all the files via http
# ------------------------------------------------------------------------------

echo "Upload files into store via the http server"
listcontent=`find $tmp_dir -type f -name *.txt -exec curl -F name=@{} $host_and_port/store \;`

echo $listcontent | sed -r "s/\}/\\n/g" > $listfile

chk=`cat $listfile | grep id | grep name | grep timestamp | wc -l`

if [ $chk = "10" ]; then
  echo "[OK] POST /store" >> $resultfile
else
  printf "POST /store ${RED}TEST FAILED${NC}\n"
  exit 1
fi

sed -i "s/\"//g" $listfile
sed -i "s/,//g" $listfile
sed -i '/^$/d' $listfile


# ------------------------------------------------------------------------------
# Get the list of ids in the local temporary dir
# ------------------------------------------------------------------------------

rm -f $allidsfile

echo "Searching for 3 most recent files in $tmp_dir"
while read p; do
  ok=0
  curl $host_and_port/files/`echo "$p" | awk '{print $3}';` | grep id | awk '{print $2}' >> $allidsfile && ok=1

  id=`echo $p | awk '{print $3 " for " $5}';`
  last_id=`echo $p | awk '{print $3}'`

  if [ $ok = "1" ]; then
    echo "[OK]  GET /files/$id" >> $resultfile
  else
    printf "GET /files/<${id}> ${RED}TEST FAILED${NC}\n"
    exit 1
  fi
done < $listfile 


sed -i "s/\"//g" $allidsfile
sed -i "s/,//g" $allidsfile
sed -i '/^$/d' $allidsfile

# ------------------------------------------------------------------------------
# Select 3 mru from local list
# ------------------------------------------------------------------------------

tail -n 3 $allidsfile | sort > $threeidsfile

echo "Getting the mrufils from http server" 
rm -f $mruidsfile


# ------------------------------------------------------------------------------
# Query the server for getting the mrufiles
# ------------------------------------------------------------------------------

curl $host_and_port/mrufiles | grep id >> $mruidsfile
sed -i "s/\"//g" $mruidsfile
sed -i "s/,//g" $mruidsfile
sed -i '/^$/d' $mruidsfile
 
cp $mruidsfile $sortedmruidsfile
cat $sortedmruidsfile | awk '{print $2}' > $mruidsfile
cat $mruidsfile | sort > $sortedmruidsfile

ok=0
diff $threeidsfile $sortedmruidsfile && ok=1

if [ $ok = "1" ]; then
  echo "[OK]  GET /mrufiles" >> $resultfile
else
  printf "GET /mrufiles ${RED}TEST FAILED${NC}\n"
  exit 1
fi


# ------------------------------------------------------------------------------
# Get a single zip file
# ------------------------------------------------------------------------------
ok=0
curl $host_and_port/files/$last_id/zip --output $tmp_dir2/$last_id.zip && ok=1
echo Downloaded $tmp_dir2/$last_id.zip

if [ $ok = "1" ]; then
  echo "[OK]  GET /files/$last_id/zip" >> $resultfile
else
  printf "GET /files/$last_id/zip ${RED}TEST FAILED${NC}\n"
  exit 1
fi

filename=`zipinfo $tmp_dir2/$last_id.zip | grep File | awk '{ print $9 }'`
ok=0
cd $tmp_dir2 && unzip $tmp_dir2/$last_id.zip && cd - && ok=1

if [ $ok = "0" ]; then
  printf "GET /files/$last_id/zip ${RED}TEST FAILED${NC}\n"
  exit 1
fi

diff $tmp_dir2/$filename $tmp_dir/$filename || ok=0
if [ $ok = "0" ]; then
  printf "GET /files/$last_id/zip ${RED}TEST FAILED${NC}\n"
  exit 1
fi


# ------------------------------------------------------------------------------
# Show results
# ------------------------------------------------------------------------------

echo ------------------------------------
printf "${GREEN}TEST SUCCEDED${NC}\n"
echo ------------------------------------
cat $resultfile

rm -rf $tmp_dir
rm -rf $tmp_dir2