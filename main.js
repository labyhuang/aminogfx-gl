'use strict';

var DEBUG = false;
var DEBUG_FPS = false;

if (DEBUG) {
    console.log('inside of the aminogfx main.js');
}

//var amino_core = require('aminogfx'); //NPM module
var amino_core = require('./src/core/main'); //modified code

//load native module
var binary = require('node-pre-gyp');
var path = require('path');
var binding_path = binary.find(path.resolve(path.join(__dirname, 'package.json')));
var sgtest = require(binding_path);

var shaders = require('./src/shaders.js');
var fs = require('fs');

//detect platform
var OS;

if (process.arch == 'arm') {
    OS = 'RPI';
}

if (process.platform == 'darwin') {
    OS ='MAC';
}

//AminoGfx
var AminoGfx = sgtest.AminoGfx;

/**
 * Initialize AminoGfx instance.
 */
AminoGfx.prototype.init = function () {
    if (DEBUG) {
        console.log('AminoGfx.init()');
    }

    //initialize bindings
    amino_core.makeProps(this, {
        //position (window only)
        x: 0,
        y: 0,

        //QHD size
        w: 640,
        h: 360,

        //color
        opacity: 1,
        fill: '#000000'
    });

    /* TODO conversion
    this.fill.convert = function () {
        var color = amino_core.ParseRGBString(stage.fill());

        gl_native.updateWindowProperty(stage, 'r', color.r);
        gl_native.updateWindowProperty(stage, 'g', color.g);
        gl_native.updateWindowProperty(stage, 'b', color.b);
    });
    */

    //TODO more
    //cbx

    //fonts
    /*
    fontmap['source']  = new JSFont(defaultFonts['source']);
    fontmap['awesome'] = new JSFont(defaultFonts['awesome']);

    core.defaultFont = fontmap['source'];
    */

    //root wrapper
    //this.setRoot(this.createGroup()); cbxx
};

AminoGfx.prototype.start = function (done) {
    var self = this;

    //preload shaders
    shaders.preloadShaders(OS, function (err) {
        if (err) {
            done(err);
            return;
        }

        //pass to native code
        self._start(function (err) {
            if (err) {
                done.call(self, err);
                return;
            }

            //init shaders
            self.GL = AminoGfx.GL;
            shaders.init(self, OS);

            //ready (Note: this points to the instance)
            done.call(self, err);

            //check root
            /* cbxx
            if (!this.root) {
                throw new Error('Missing root!');
            }
            */

            //start rendering loop
            self.startTimer();
        });
    });
};

AminoGfx.prototype.setRoot = function (root) {
    this.root = root;

    this._setRoot(root); //TODO cbx
};

AminoGfx.prototype.startTimer = function () {
    if (this.timer) {
        return;
    }

    var self = this;

    function immediateLoop() {
        self.tick();

        //debug: fps
        if (DEBUG_FPS) {
            var time = (new Date()).getTime();

            if (self.lastTime) {
                console.log('fps: ' + (1000 / (time - self.lastTime)));
            }

            self.lastTime = time;
        }

        //next cycle
        self.timer = setImmediate(immediateLoop);
    }

    //see https://nodejs.org/api/timers.html#timers_setimmediate_callback_arg
    this.timer = setImmediate(immediateLoop);
};

AminoGfx.prototype.handleEvent = function (evt) {
    //debug
    //console.log('Event: ' + JSON.stringify(evt));

    evt.time = new Date().getTime();

    exports.input.processEvent(this, evt);
};

AminoGfx.prototype.destroy = function () {
    if (this.timer) {
        clearImmediate(this.timer);
        this.timer = null;
    }

    this._destroy();
};

exports.AminoGfx = AminoGfx;

//AminoImage
var AminoImage = sgtest.AminoImage;

Object.defineProperty(AminoImage.prototype, 'src', {
    set: function (src) {
        //check file
        if (typeof src == 'string') {
            var self = this;

            fs.readFile(src, function (err, data) {
                //check error
                if (err) {
                    if (self.onload) {
                        self.onload(err);
                    }

                    return;
                }

                //get image
                self.loadImage(data, function (err, img) {
                    //call onload
                    if (self.onload) {
                        self.onload(err, img);
                    }
                });
            });

            return;
        }

        //convert buffer
        if (!Buffer.isBuffer(src)) {
            if (this.onload) {
                this.onload(new Exception('buffer expected!'));
            }

            return;
        }

        //native call
        this.loadImage(src, this.onload);
    }
});

exports.AminoImage = AminoImage;

var Core = amino_core.Core;

//Fonts

var fontmap = {};
var defaultFonts = {
    'source': {
        name: 'source',
        weights: {
            200: {
                normal: 'SourceSansPro-ExtraLight.ttf',
                italic: 'SourceSansPro-ExtraLightItalic.ttf'
            },
            300: {
                normal: 'SourceSansPro-Light.ttf',
                italic: 'SourceSansPro-LightItalic.ttf'
            },
            400: {
                normal: 'SourceSansPro-Regular.ttf',
                italic: 'SourceSansPro-Italic.ttf'
            },

            600: {
                normal: 'SourceSansPro-Semibold.ttf',
                italic: 'SourceSansPro-SemiboldItalic.ttf'
            },
            700: {
                normal: 'SourceSansPro-Bold.ttf',
                italic: 'SourceSansPro-BoldItalic.ttf'
            },
            900: {
                normal: 'SourceSansPro-Black.ttf',
                italic: 'SourceSansPro-BlackItalic.ttf'
            }
        }
    },
    'awesome': {
        name: 'awesome',
        weights: {
            400: {
                normal: 'fontawesome-webfont.ttf'
            }
        }
    }
};

/**
 * Native property ids.
 *
 * Note: value has to match native code value
 */
var propsHash = {

    //general
    visible: 18,
    opacity: 27,
    r: 5,
    g: 6,
    b: 7,
    texid: 8,
    w: 10,
    h: 11,
    x: 21,
    y: 22,

    //transforms  (use x and y for translate in X and Y)
    sx: 2,
    sy: 3,
    rz: 4,
    rx: 19,
    ry: 20,

    //text
    text: 9,
    fontSize: 12,
    fontId: 28,
    vAlign: 40,
    wrap: 41,

    //animation
    count: 29,
    lerpprop: 16,
    autoreverse: 35,
    then: 37,
    stop: 38,

    //geometry
    geometry:  24,
    filled:    25,
    closed:    26,
    dimension: 36,

    //rectangle texture
    textureLeft:   30,
    textureRight:  31,
    textureTop:    32,
    textureBottom: 33,

    //clipping
    cliprect: 34
};

/**
 * Time function native property values.
 */
var timeFuncsHash = {
    //time function
    linear:     0x0,
    cubicIn:    0x1,
    cubicOut:   0x2,
    cubicInOut: 0x3
};

/**
 * Vertial text alignment property values.
 */
var textVAlignHash = {
    baseline: 0x0,
    top:      0x1,
    middle:   0x2,
    bottom:   0x3
};

/**
 * Text wrapping property values.
 */
var textWrapHash = {
    none: 0x0,
    end:  0x1,
    word: 0x2
};

/**
 * JSFont constructor.
 */
function JSFont(desc) {
    this.name = desc.name;

    //regular
    var reg = desc.weights[400];

    //properties
    this.desc = desc;
    this.weights = {};
    this.filepaths = {};

    //path
    var aminodir = path.join(__dirname, 'resources/');

    if (desc.path) {
        aminodir = desc.path;
    }

    //iterate weights
    for (var weight in desc.weights) {
        //load normal style
        //FIXME style not used
        var filepath = path.join(aminodir, desc.weights[weight].normal);

        if (!fs.existsSync(filepath)) {
            throw new Error('WARNING. File not found: ' + filepath);
        }

        this.weights[weight] = Core.getCore().getNative().createNativeFont(filepath);
        this.filepaths[weight] = filepath;
    }

    this.getNative = function (size, weight, style) {
        //FIXME style not used; size not used
        if (this.weights[weight]) {
            return this.weights[weight];
        }

        console.log('ERROR. COULDN\'T find the native for ' + size + ' ' + weight + ' ' + style);

        //return regular
        return this.weights[400];
    };

    /**
     * @func calcStringWidth(string, size, weight, style)
     *
     * returns the width of the specified string rendered at the specified size
     */
    this.calcStringWidth = function (str, size, weight, style) {
        return sgtest.getCharWidth(str, size, this.getNative(size, weight, style));
    };

    /**
     * Get font height.
     */
    this.getHeight = function (size, weight, style) {
        if (!size) {
            throw new Error('SIZE IS UNDEFINED');
        }

        return sgtest.getFontHeight(size, this.getNative(size, weight, style));
    };

    /**
     * Get font height metrics.
     *
     * Returns ascender & descender values.
     */
    this.getHeightMetrics = function (size, weight, style) {
        if (!size) {
            throw new Error('SIZE IS UNDEFINED');
        }

        return {
            ascender: sgtest.getFontAscender(size, this.getNative(size, weight, style)),
            descender: sgtest.getFontDescender(size, this.getNative(size, weight, style))
        };
    };
}

/**
 * Animated property.
 */
function JSPropAnim(target, name) {
    this._from = null;
    this._to = null;
    this._duration = 1000;
    this._loop = 1;
    this._delay = 0;
    this._autoreverse = 0;
    this._lerpprop = timeFuncsHash.cubicInOut;
    this._then_fun = null;

    //setters
    this.from  = function(val) {  this._from = val;        return this;  };
    this.to    = function(val) {  this._to = val;          return this;  };
    this.dur   = function(val) {  this._duration = val;    return this;  };
    this.delay = function(val) {  this._delay = val;       return this;  };
    this.loop  = function(val) {  this._loop = val;        return this;  };
    this.then  = function(fun) {  this._then_fun = fun;    return this;  };
    this.autoreverse = function(val) { this._autoreverse = val ? 1:0; return this;  };
    this.timeFunc = function (val) {
        var tf = timeFuncsHash[val];

        if (!tf) {
            throw new Error('unknown time function: ' + val);
        }

        this._lerpprop = tf;

        return this;
    };

    /*
     * Start the animation.
     */
    this.start = function () {
        if (DEBUG) {
            console.log('startin anim');
        }

        var self = this;

        setTimeout(function () {
            if (DEBUG) {
                console.log('after delay. making it.');
            }

            //create native instance
            var core = Core.getCore();
            var nat = core.getNative();

            self.handle = nat.createAnim(target.handle, name, self._from,self._to,self._duration);

            if (DEBUG) {
                console.log('handle is: ', self.handle);
            }

            //set params
            nat.updateAnimProperty(self.handle, 'count', self._loop);
            nat.updateAnimProperty(self.handle, 'autoreverse', self._autoreverse);
            nat.updateAnimProperty(self.handle, 'lerpprop', self._lerpprop);
            nat.updateAnimProperty(self.handle, 'then', self._then_fun);

            //add
            core.anims.push(self);
        }, this._delay);

        return this;
    };

    /**
     * Stop the animation.
     */
    this.stop = function () {
        Core.getCore().getNative().updateAnimProperty(this.handle, 'stop', true);
    };

    //TODO more features from native

}

//native bindings

var PNG_HEADER = new Buffer([ 137, 80, 78, 71, 13, 10, 26, 10 ]); //PNG Header

var gl_native = {
    createNativeFont: function (filename) {
        return sgtest.createNativeFont(filename, path.join(__dirname, '/resources/shaders'));
    },
    registerFont: function (args) {
        fontmap[args.name] = new JSFont(args);
    },
    getRegisteredFonts: function () {
        return fontmap;
    },
    getFont: function (name) {
        return fontmap[name];
    },
    textVAlignHash: textVAlignHash,
    textWrapHash: textWrapHash,
    getTextLineCount: function (handle) {
        if (handle == undefined) {
            throw new Error('Can\'t set a property on an undefined handle!!');
        }

        return sgtest.getTextLineCount(handle);
    },
    getTextHeight: function (handle) {
        if (handle == undefined) {
            throw new Error('Can\'t set a property on an undefined handle!!');
        }

        return sgtest.getTextHeight(handle);
    },
    updateProperty: function (handle, name, value) {
        if (handle == undefined) {
            throw new Error('Can\'t set a property on an undefined handle!!');
        }

        //console.log('setting', handle, name, propsHash[name], value, typeof value);

        var hash = propsHash[name];

        if (!hash) {
            throw new Error('Unknown update property: ' + name);
        }

        sgtest.updateProperty(handle, hash, value);
    },
    setRoot: function (handle) {
        return  sgtest.addNodeToGroup(handle, this.rootWrapper);//cbxx
    },
    createRect: function (hasImage) {
        return sgtest.createRect(hasImage);
    },
    createGroup: function () {          return sgtest.createGroup();          },
    createPoly: function ()  {          return sgtest.createPoly();           },
    createText: function () {           return sgtest.createText();           },
    addNodeToGroup: function (h1,h2) {
        return sgtest.addNodeToGroup(h1,h2);
    },
    removeNodeFromGroup: function (h1, h2) {
        return sgtest.removeNodeFromGroup(h1, h2);
    },
    loadImage: function (src, cb) {
        var img = new AminoImage();

        img.onload = function (err) {
            if (err) {
                if (DEBUG) {
                    console.log('could not load image');
                }

                cb();
                return;
            }

            //debug
            //console.log('image buffer: w=' + ibuf.w + ' h=' + ibuf.h + ' bpp=' + ibuf.bpp + ' len=' + ibuf.buffer.length);

            //load texture
            var nat = Core.getCore().getNative();

            //TODO refactor (async, must be called in OpenGL loop)
            nat.loadBufferToTexture(-1, img.w, img.h, img.bpp, img.buffer, function (texture) {
                cb(texture);
            });
        };

        img.src = src;
    },
    loadBufferToTexture: function (texid, w, h, bpp, buf, cb) {
        return cb(sgtest.loadBufferToTexture(texid, w,h, bpp, buf));
    },
    createAnim: function (handle, prop, start, end, dur) {
        var hash = propsHash[prop];

        if (!hash) {
            throw new Error('invalid native property name', prop);
        }

        return sgtest.createAnim(handle, hash, start, end, dur);
    },
    createPropAnim: function (obj, name) {
        return new JSPropAnim(obj, name);
    },
    updateAnimProperty: function (handle, prop, type) {
        var hash = propsHash[prop];

        if (!hash) {
            throw new Error('invalid animattion property: ' + prop);
        }

        sgtest.updateAnimProperty(handle, hash, type);
    }
};

exports.input = amino_core.input;

exports.makeProps = amino_core.makeProps;

//basic
exports.Rect      = amino_core.Rect;
exports.Group     = amino_core.Group;
exports.Circle    = amino_core.Circle;
exports.Polygon   = amino_core.Polygon;
exports.Text      = amino_core.Text;
exports.ImageView = amino_core.ImageView;

//extended
exports.PixelView    = amino_core.PixelView;
exports.RichTextView = amino_core.RichTextView;

//initialize input handler
exports.input.init(OS);

//exports.PureImageView = amino.primitives.PureImageView;
//exports.ConstraintSolver = require('./src/ConstraintSolver');
