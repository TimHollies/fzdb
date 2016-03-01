#include "StringMapSerialiser.h"
#include <cstring>

#include "spdlog/spdlog.h"

struct SerialHeader
{
	std::size_t size;	// Total serialised size in bytes.
	std::size_t count;	// Number of headers to expect after this header.
};

struct EntryHeader
{
	std::size_t offset;		// Offset from the beginning of the entire serialisation.
	std::size_t stringSize;	// Serialised size of the string.
};

StringMapSerialiser::StringMapSerialiser(std::map<std::string, unsigned int> *map) : _map(map)
{

}

std::size_t StringMapSerialiser::serialise(Serialiser &serialiser) const
{
	std::size_t origSize = serialiser.size();

	// Create a header.
	SerialHeader header;
	Serialiser::zeroBuffer(&header, sizeof(SerialHeader));
	header.size = 0;	// We don't know this yet.
	header.count = _map->size();

	std::vector<Serialiser::SerialProperty> propList;
	propList.push_back(Serialiser::SerialProperty(&header, sizeof(SerialHeader)));

	// Push back the appropriate amount of dummy entry headers.
	EntryHeader eHeader;
	Serialiser::zeroBuffer(&eHeader, sizeof(EntryHeader));
	for ( int i = 0; i < header.count; i++ )
	{
		propList.push_back(Serialiser::SerialProperty(&eHeader, sizeof(EntryHeader)));
	}

	// Serialise all of these properties.
	serialiser.serialise(propList);
	std::size_t dataBegin = serialiser.size();
	std::size_t headerChunkSize = dataBegin - origSize;

	// Serialise each entry.
	EntryHeader* ehBase = serialiser.reinterpretCast<EntryHeader*>(origSize + sizeof(SerialHeader));
	std::size_t bytesSerialised = 0;
	int i = 0;
	for ( auto it = _map->cbegin(); it != _map->cend(); ++it )
	{
		std::size_t ourOffset = serialiser.size() - dataBegin;\
		const std::string &str = it->first;
		unsigned int id = it->second;

		std::size_t serialisedThisLoop = serialiser.serialise(Serialiser::SerialProperty(&id, sizeof(unsigned int)));

		// We use size+1 to ensure that a null terminator is present in the serialised stream.
		char* buffer = new char[str.size() + 1];
		memset(buffer, 0, str.size()+1);
		strcpy(buffer, str.c_str());
		serialisedThisLoop += serialiser.serialise(Serialiser::SerialProperty(buffer, str.size()+1));
		delete buffer;

		EntryHeader* pHeader = &(ehBase[i]);
		pHeader->stringSize = str.size()+1;
		pHeader->offset = ourOffset;

		bytesSerialised += serialisedThisLoop;
		i++;
	}

	SerialHeader* pHeader = serialiser.reinterpretCast<SerialHeader*>(origSize);
	pHeader->size = headerChunkSize + bytesSerialised;

	return serialiser.size() - origSize;
}

void StringMapSerialiser::unserialise(const char *serialisedData, std::size_t length)
{
	const SerialHeader* pHeader = reinterpret_cast<const SerialHeader*>(serialisedData);
	if ( pHeader->size != length )
	{
	    throw InvalidStringTableException("Internal string table size does not match size of data.");
	}

	const EntryHeader* eHeader = reinterpret_cast<const EntryHeader*>(serialisedData + sizeof(SerialHeader));
	for ( int i = 0; i < pHeader->count; i++ )
	{
		const EntryHeader* e = &(eHeader[i]);
		if ( (const char*)e - serialisedData >= length )
		    throw InvalidStringTableException("Header start for string " + std::to_string(i) + " exceeds length of data.");
		
		const char* data = serialisedData + e->offset;
		if ( data - serialisedData >= length )
		    throw InvalidStringTableException("Data start for string " + std::to_string(i) + " exceeds length of data.");
		
		if ( (std::size_t)(serialisedData + e->offset + e->stringSize) > length )
		    throw InvalidStringTableException("Length of string " + std::to_string(i) + " exceeds length of data.");
		
		if ( data + e->stringSize - 1 != '\0' )
		    throw InvalidStringTableException("String " + std::to_string(i) + " is not null-terminated.");
		
		const unsigned int* id = reinterpret_cast<const unsigned int*>(data);
		data += sizeof(unsigned int);

		_map->insert(std::pair<std::string, unsigned int>(std::string(data), *id));
	}
}
