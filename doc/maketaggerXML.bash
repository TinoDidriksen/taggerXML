METH=git
METH=https

if [ ! -d hashmap ]; then
    mkdir hashmap
    cd hashmap
    git init
    git remote add origin $METH://github.com/kuhumcst/hashmap.git
    cd ..
fi
cd hashmap
git pull origin master
cd ..

if [ ! -d letterfunc ]; then
    mkdir letterfunc
    cd letterfunc
    git init
    git remote add origin $METH://github.com/kuhumcst/letterfunc.git
    cd ..
fi
cd letterfunc
git pull origin master
cd ..

if [ ! -d parsesgml ]; then
    mkdir parsesgml
    cd parsesgml
    git init
    git remote add origin $METH://github.com/kuhumcst/parsesgml.git
    cd ..
fi
cd parsesgml
git pull origin master
cd ..

if [ ! -d taggerXML ]; then
    mkdir taggerXML
    cd taggerXML
    git init
    git remote add origin $METH://github.com/kuhumcst/taggerXML.git
    cd ..
fi
cd taggerXML
git pull origin master
cd src
make all
cd ..
cd ..
