#include "xmlcachehandler.h"
#include <QFile>
#include "../dal/xmlrequests/retrieveindex.h"
#include "../dal/connectiondata.h"
#include "../dal/xmlrequests/retrievedirectory.h"

/*
	Constructors
*/
XMLCacheHandler::XMLCacheHandler(ConnectionData* _cd, QObject* parent) : QObject(parent)
{
	conndata = _cd;
	gotConnData = true;
	cacheFile = 0;
}
XMLCacheHandler::XMLCacheHandler(QObject* parent) : QObject(parent)
{
	gotConnData = false;
	cacheFile = 0;
}

/*
	Public Functions
*/

/*
	Requests
*/

void XMLCacheHandler::requestArtistList()
{
	connect(this, SIGNAL(cacheReady()), this, SLOT(returnArtistElement()));
	loadCache();
}
void XMLCacheHandler::requestArtistAlbums(QString _artistName)
{
	QDomElement artist = findArtist(_artistName);
	if(artist.elementsByTagName("album").count() > 0)
	{
		emit takeThisArtistDirectoryAwayItsJustGettingInTheWay(artist);
		printf("cache already contains albums by " + _artistName.toLocal8Bit() + ".\n");
		return;
	}
	QString id = artist.attribute("id","null");
	if(id.compare("null") != 0)
	{
		RetrieveDirectory* rd = new RetrieveDirectory(conndata,id,this);
		connect(rd,SIGNAL(gedditWhileItsHot(QDomDocument*)),this,
			SLOT(recievedArtistsDir(QDomDocument*)));
		rd->retrieve();
	}
	else
		emit requireHardReset();
}
void XMLCacheHandler::requestAlbum(QString _artistName, QString _albumName)
{
	QDomElement artist = findArtist(_artistName);
	QDomElement album = getFirstChildByAttributeValue(artist,"title",_albumName);
	if(album.elementsByTagName("track").count() > 0)
	{
		emit takeThisAlbumWhileStocksLast(album);
		printf("cache already contains album " + _albumName.toLocal8Bit() + ".\n");
		return;
	}
	QString id = album.attribute("id","null");
	if(id.compare("null") != 0)
	{
		RetrieveDirectory* rd = new RetrieveDirectory(conndata,id,this);
		connect(rd,SIGNAL(gedditWhileItsHot(QDomDocument*)),this,
			SLOT(recievedAlbum(QDomDocument*)));
		rd->retrieve();
	}
	else
		emit requireHardReset();
}
void XMLCacheHandler::hardResetCache()
{
	// check for conection data
	if(gotConnData)
	{
		if(cacheFile != 0)
		{
			delete cacheFile;
			cacheFile = 0;
		}

		RetrieveIndex* ri = new RetrieveIndex(conndata,this->parent());
		connect(ri,SIGNAL(gedditWhileItsHot(QDomDocument*)),this, SLOT(saveNewCache(QDomDocument*)));
		ri->retrieve();

	}
	else
	{
		printf("cannot request fresh cache,"
			" don't have connection data.\n");
		emit noConnectionData();
	}
}

/*
	Private Slots
*/

void XMLCacheHandler::saveNewCache(QDomDocument* _responsexml)
{
	sender()->deleteLater();

    cacheFile = _responsexml;

	saveCacheToDisk();

	emit cacheReady();
}
void XMLCacheHandler::returnArtistElement()
{
	//disconnect the signal to ensure we don't start sending hundreds of ready signals
	disconnect(this, SIGNAL(cacheReady()), this, SLOT(returnArtistElement()));

	//emit the signal that its ready
	emit takeThisIndexOffMeItsCrampingMyStyle(cacheFile->firstChildElement().firstChildElement());
}
void XMLCacheHandler::recievedArtistsDir(QDomDocument* _respXML)
{
	sender()->deleteLater();

	QDomNodeList qnl = _respXML->elementsByTagName("child");
	QDomElement parent = findArtist(_respXML->elementsByTagName("directory")
		.at(0).toElement().attribute("name","NULL"));
	if(!parent.isNull())
	{
		while(qnl.count() != 0)
		{
			qnl.at(0).toElement().setTagName("album");
			qnl.at(0).toElement().removeAttribute("parent");
			//qnl.at(0).toElement().removeAttribute("artist");
			qnl.at(0).toElement().removeAttribute("coverArt");
			parent.appendChild(qnl.at(0));
		}

		delete _respXML;
		//puts(cacheFile->toString().toLocal8Bit());
		saveCacheToDisk();
	}

	emit takeThisArtistDirectoryAwayItsJustGettingInTheWay(parent);
}
void XMLCacheHandler::recievedAlbum(QDomDocument* _respXML)
{
	sender()->deleteLater();

	QString artistName = _respXML->firstChildElement().firstChildElement()
		.firstChildElement().attribute("artist","NULL");
	QString albumName = _respXML->firstChildElement().firstChildElement()
		.firstChildElement().attribute("album","NULL");

	QDomNodeList qnl = _respXML->elementsByTagName("child");
	QDomElement parent = findArtist(artistName);
	QDomElement album = getFirstChildByAttributeValue(parent,"title",albumName);

	if(!parent.isNull())
	{
		while(qnl.count() != 0)
		{
			qnl.at(0).toElement().setTagName("track");
			qnl.at(0).toElement().removeAttribute("parent");
			qnl.at(0).toElement().removeAttribute("path");
			qnl.at(0).toElement().removeAttribute("coverArt");
			album.appendChild(qnl.at(0));
			
		}

		delete _respXML;
		//puts(cacheFile->toString().toLocal8Bit());
		saveCacheToDisk();
	}

	emit takeThisAlbumWhileStocksLast(album);
}

/*
	Private Functions
*/


/*
	IO
*/
void XMLCacheHandler::loadCache()
{
	bool valid = true;
	//if i dont have connection data don't bother
	if(!gotConnData)
	{
		emit noConnectionData();
		return;
	}
	//if i do but the cache hasn't been loaded into mem
	else if(cacheFile == 0)
	{
		//try and load if
		if(loadCacheFromDisk())
		{
			//if it loads emit ready
			emit cacheReady();
			return;
		}
		//if it doesnt say the hard copy is corrupt
		else
			valid = false;
	}
	//if it wouldn't load or the one in memory is empty try and reload
	if(!valid || cacheFile->elementsByTagName("artist").count() == 0)
		hardResetCache();
	//else the cache in memory is fine
	else
		emit cacheReady();
}
bool XMLCacheHandler::loadCacheFromDisk()
{
	//if the cachefile isnt empty
	if(cacheFile != 0)
		delete cacheFile;

	//open the cache in readonly
	QFile file("xmlcache.xml");
	file.open(QIODevice::ReadOnly | QIODevice::Text);
	cacheFile = new QDomDocument("cache");

	//try and parse the file
	bool succ = cacheFile->setContent(file.readAll());
	
	file.close();
	
	//if the parsing failed delete the cache
	if(!succ)
	{
		delete cacheFile;
		cacheFile = 0;
		emit requireHardReset();
	}

	return succ;
	
}
bool XMLCacheHandler::saveCacheToDisk()
{
	if(cacheFile != 0)
	{
		QFile file("xmlcache.xml");
		file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate);
                file.remove();

		//TODO -- this should use a text stream writer to save
                file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate);
		file.write(cacheFile->toString(3).toUtf8());
		file.flush();
		file.close();
		printf("saved new cache\n");
		return true;
	}
	return false;
	
}

/*
	Cache Queries
*/


QDomElement XMLCacheHandler::findArtist(QString _name)
{
	
	//probably needlessly check cache
	if(cacheFile == 0)
	{
		loadCacheFromDisk();
	}
	if(cacheFile != 0)
	{
		//don't search all artist elements in cache just the index
		//corresponding to the requested name
		QString firstLetter = QString(_name.toLocal8Bit().at(0));

		//stupid special case for 'The'
		if(firstLetter.compare("T") == 0 &&  QString(_name.toLocal8Bit().at(1)).compare("h") == 0 &&
				QString(_name.toLocal8Bit().at(2)).compare("e") == 0 && QString(_name.toLocal8Bit().at(3)).compare(" ") == 0)
			firstLetter = QString(_name.toLocal8Bit().at(4));

		//allows for artists starting with a number to be obtained
		bool isNumber = false;
		firstLetter.toInt(&isNumber);
		firstLetter = isNumber ? QString("#") : firstLetter;
 
		//get the child of the index element
		QDomElement index = getFirstChildByAttributeValue(cacheFile->
			elementsByTagName("indexes").at(0).toElement(),"name", firstLetter);

		if(!index.isNull())
		{
			//return the artist with matching name
			QDomElement artist = getFirstChildByAttributeValue(index, "name", _name);
			return artist;
		}
		else
			return QDomElement();
	}
	else
	{
		//TODO-- if we get here need a hard reset
		printf("I've lost the cache...\n");
		return QDomElement();
	}
}

/*
	DOM helper functions
*/

QDomElement XMLCacheHandler::getFirstChildByAttributeValue(QDomElement _toSearch,
	QString _attrib, QString _value)
{
	for(int ctr = 0; ctr < _toSearch.childNodes().count(); ++ctr)
	{
		//iterate through child nodes
		QDomNode qdm = _toSearch.childNodes().at(ctr);
		//check its an element
		if(qdm.isElement())
		{
			//if element attribute value matches return
			if(qdm.toElement().attribute(_attrib,"-").compare(_value) == 0)
			{
				return qdm.toElement();
			}
		}
	}
	return QDomElement();
}


/*
	Destructor
*/
XMLCacheHandler::~XMLCacheHandler(){

	if(cacheFile != 0)
	{
		delete cacheFile;
	}

}