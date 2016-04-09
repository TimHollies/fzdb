#ifndef FUZZY_MODEL_TYPES_AUTHORID
#define FUZZY_MODEL_TYPES_AUTHORID

#include <string>

#include "./UInt.h"
#include <iostream>

namespace model {
    namespace types {

        // Stores an integer value.
        class AuthorID : public UInt {      

        public:

            AuthorID() : UInt()
            {

            }
            
            AuthorID(uint32_t value) :
                UInt(value)
            {
            }            
           
			void setupDefaultMetaData(const unsigned char confidence) override;

            virtual ~AuthorID() {}            

            virtual std::shared_ptr<Base> Clone() override {
                auto cloned = std::make_shared<AuthorID>();
				cloned->_value = _value;
				copyValues(cloned);
				return cloned;
            }    

			unsigned char confidence() const override {
				return 100;
			}

        protected:

            AuthorID(const char* &serialisedData, std::size_t length) : UInt(serialisedData, length)
            {
            }
        };
    }
}


#endif // !FUZZY_MODEL_TYPES_AUTHORID
