date=$(git log --pretty=format:"%H @ %ad" -n 1)
tags=$(git describe --always --tags)
branch=$(git symbolic-ref HEAD 2> /dev/null | cut -b 12-)
if [ "master" = $branch ]
then
    branch=""
else
    branch="-dev"
fi

echo "Date:" "$date"
echo "Version:" "$tags"
sed -e "s/@VERSION@/$tags/" -e "s/@DATE@/$date/" "$1" > "$2"
