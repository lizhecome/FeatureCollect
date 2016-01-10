//
//  SerializeMat.h
//  opencvtest
//
//  Created by lizhe on 15/9/12.
//  Copyright © 2015年 TJGD. All rights reserved.
//

#ifndef SerializeMat_h
#define SerializeMat_h

#include <iostream>
#include <fstream>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/split_free.hpp>
#include <boost/serialization/vector.hpp>

BOOST_SERIALIZATION_SPLIT_FREE(cv::Mat)
namespace boost {
    namespace serialization {
        
        /*** Mat ***/
        template<class Archive>
        void save(Archive & ar, const cv::Mat& m, const unsigned int version)
        {
            size_t elemSize = m.elemSize(), elemType = m.type();
            
            ar & m.cols;
            ar & m.rows;
            ar & elemSize;
            ar & elemType; // element type.
            size_t dataSize = m.cols * m.rows * m.elemSize();
            
            //cout << "Writing matrix data rows, cols, elemSize, type, datasize: (" << m.rows << "," << m.cols << "," << m.elemSize() << "," << m.type() << "," << dataSize << ")" << endl;
            
            for (size_t dc = 0; dc < dataSize; ++dc) {
                ar & m.data[dc];
            }
        }
        
        template<class Archive>
        void load(Archive & ar, cv::Mat& m, const unsigned int version)
        {
            int cols, rows;
            size_t elemSize, elemType;
            
            ar & cols;
            ar & rows;
            ar & elemSize;
            ar & elemType;
            
            m.create(rows, cols, elemType);
            size_t dataSize = m.cols * m.rows * elemSize;
            
            //cout << "reading matrix data rows, cols, elemSize, type, datasize: (" << m.rows << "," << m.cols << "," << m.elemSize() << "," << m.type() << "," << dataSize << ")" << endl;
            
            for (size_t dc = 0; dc < dataSize; ++dc) {
                ar & m.data[dc];
            }
        }
        
    }
}

#endif /* SerializeMat_h */
