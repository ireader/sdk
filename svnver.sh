svninfo=$(svn info -r HEAD)
lastchangerev=$(printf "%s\n" "$svninfo" | awk 'NR==8' | awk -F ': ' '{print $2}')
lastchangedate=$(printf "%s\n" "$svninfo" | awk 'NR==9' | awk -F ': ' '{print $2}')
echo "Last Changed Rev:" "$lastchangerev"
echo "Last Changed Date:" "$lastchangedate"
sed -e 's/\$WCREV\$/'"$lastchangerev"'/' -e 's/\$WCDATE\$/'"$lastchangedate"'/' -e 's/\$WCNOW\$/'"$(date)"'/' "$1" > "$2"