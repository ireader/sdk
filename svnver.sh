svninfo=$(svn info -r HEAD)
if [ "11" = $(svn info | wc -l) ]
then
# svn 1.6.x
	lastchangerev=$(printf "%s\n" "$svninfo" | awk 'NR==8' | awk -F ': ' '{print $2}')
	lastchangedate=$(printf "%s\n" "$svninfo" | awk 'NR==9' | awk -F ': ' '{print $2}')
else
# svn 1.8.x
	lastchangerev=$(printf "%s\n" "$svninfo" | awk 'NR==9' | awk -F ': ' '{print $2}')
	lastchangedate=$(printf "%s\n" "$svninfo" | awk 'NR==10' | awk -F ': ' '{print $2}')
fi
echo "Last Changed Rev:" "$lastchangerev"
echo "Last Changed Date:" "$lastchangedate"
sed -e 's/\$WCREV\$/'"$lastchangerev"'/' -e 's/\$WCDATE\$/'"$lastchangedate"'/' -e 's/\$WCNOW\$/'"$(date)"'/' "$1" > "$2"
