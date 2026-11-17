#ifndef TEXTURE_ATLAS_H
#define TEXTURE_ATLAS_H

#include <cassert>
#include <iostream>

#include "vector_util.h"
#include <SFML/Graphics.hpp>

#include <algorithm>
#include <vector>

// RectPacker maintains two offsets into an overall bin size:
//  - A line y-offset, representing the current horizontal line on which to place new rectangles
//      horizontally along (the line corresponding to the rectangle tops)
//  - An x-offset into the line, representing the x-coordinate to align the next rectangle left-side
// Furthermore it maintains the maximum texture height encountered along the current line.
// The RectPacker places rectangles left to right along this line until it cannot fit the next
//  rectangle. Once this happens, the line is shifted down to meet the bottom of the tallest texture
//  placed on the line, and the x-offset is reset. This continues until the next rectangle
//  packed cannot be fit on the line within the bin space.
class RectPacker {
public:
    RectPacker(const sf::Vector2f& size) : mSize(size) {};

    // pack returns { succeeded, rect id }. If not succeeded, id is irrelevant (but ideally should
    // be set to the maximum possible value).
    std::pair<bool, size_t> pack(const sf::Vector2f& rectSize) { 
        // move to the next row if it doesn't fit horizontally
        if (mX + rectSize.x > mSize.x) {
            mX = 0;
            mY += mRowHeight;
            mRowHeight = 0;
        }

        // check if the rectangle fits inside the atlas
        if (mX + rectSize.x > mSize.x || mY + rectSize.y > mSize.y) {
            return {false, static_cast<size_t>(-1)};
        }

        // store rectangle at the current position
        mRects.emplace_back(sf::Vector2f(mX, mY), rectSize);

        // get its ID
        size_t id = mRects.size() - 1;

        // move the horizontal position forward
        mX += rectSize.x;

        // remember the tallest rectangle in this row
        mRowHeight = std::max(mRowHeight, rectSize.y);

        return {true, id};

     }

    // Returns the coordinates and size of a previously returned rectangle id.
    // Note that in this coordinate system, y points downwards from the origin at the top-left.
    sf::FloatRect getBounds(size_t rectID) const { 
        return mRects[rectID];
    }

private:
    sf::Vector2f mSize;
    float mX = 0;
    float mY = 0;
    float mRowHeight = 0;
    std::vector<sf::FloatRect> mRects;

};

// Returns a new image which pages the given img with *pixels* pixels on each side.
// The colour of new border pixels are taken by sampling the closest valid pixel in the original
// image.
//
// NOTE: a production implementation might also support other edge wrapping behaviors, like cycling
// around to the other side of the texture. See https://learnopengl.com/Getting-started/Textures
// Our implementation corresponds to GL_CLAMP_TO_EDGE
inline sf::Image padImage(const sf::Image& img, uint pixels) { 
    // get the original image size
    sf::Vector2u size = img.getSize();

    // create a larger image with padding on all sides
    sf::Image padded(
        {size.x + 2 * pixels, size.y + 2 * pixels},
        sf::Color::Transparent
    );

    // go through every pixel in the padded image
    for (unsigned int y = 0; y < padded.getSize().y; ++y) {
        for (unsigned int x = 0; x < padded.getSize().x; ++x) {

            // find the closest x coordinate in the original image
            unsigned int srcX = 0;
            if (x > pixels) {
                srcX = x - pixels;
            }

            if (srcX >= size.x) {
                srcX = size.x - 1;
            }

            // find the closest y coordinate in the original image
            unsigned int srcY = 0;
            if (y > pixels) {
                srcY = y - pixels;
            }

            if (srcY >= size.y) {
                srcY = size.y - 1;
            }

            // copy the pixel color to prevent texture bleeding
            padded.setPixel({x, y}, img.getPixel({srcX, srcY}));
        }
    }
    return padded;

};

using TextureID = size_t;

class TextureAtlas {
public:
    TextureAtlas(sf::Vector2u binSize, size_t padding)
        : mTexture(binSize), mPacker(sf::Vector2f(mTexture.getSize())), mPadding{padding} {}

    TextureID registerTexture(const std::string& filepath) { 
        // load image from file
        sf::Image img;
        if (!img.loadFromFile(filepath)) {
            return static_cast<TextureID>(-1);
        }

        // add padding around image
        sf::Image padded = padImage(img, mPadding);

        // find space for the padded image in the atlas
        auto [success, id] = mPacker.pack(sf::Vector2f(padded.getSize()));

        // get the position where the image should go
        sf::FloatRect bounds = mPacker.getBounds(id);

        // copy the padded image into the atlas
        mTexture.update(padded, sf::Vector2u(bounds.position));

        return id;
     }

    sf::FloatRect getBoundsUV(TextureID id) const { 
        // get the padded rectangle in the atlas
        sf::FloatRect bounds = mPacker.getBounds(id);

        // get the atlas size
        sf::Vector2f size(mTexture.getSize());

        // remove padding and convert the position to uv coordinates
        sf::Vector2f position((bounds.position.x + mPadding) / size.x, 
            1.f - (bounds.position.y + bounds.size.y - mPadding) / size.y
        );

        // convert the unpadded image size to uv coordinates
        sf::Vector2f uvSize(
            (bounds.size.x - 2 * mPadding) / size.x,
            (bounds.size.y - 2 * mPadding) / size.y
        );

        return sf::FloatRect(position, uvSize);
     }

    sf::Vector2u getSizePixels(TextureID id) const { 
        // get the padded rectangle size
        sf::FloatRect bounds = mPacker.getBounds(id);

        // remove padding to get the original image size
        return sf::Vector2u(
            static_cast<unsigned int>(bounds.size.x - 2 * mPadding),
            static_cast<unsigned int>(bounds.size.y - 2 * mPadding)
        );
    }

    const sf::Texture& getTexture() const { return mTexture; }

private:
    sf::Texture mTexture;
    RectPacker mPacker;
    size_t mPadding;
};

#endif  // TEXTURE_ATLAS_H
