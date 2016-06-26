'use strict';

var fs = require('fs');
var async = require('async');

var amino = require('./amino');
var PImage = require('../../pureimage/pureimage');
var comp = require('../../richtext/component');

/**
 * Convert RGB expression to object.
 *
 * Supports:
 *
 *  - hex strings
 *  - array: r, g, b (0..1)
 *  - objects: r, g, b (0..1)
 */
function ParseRGBString(Fill) {
    if (typeof Fill == 'string') {
        //strip off any leading #
        if (Fill.substring(0, 1) == '#') {
            Fill = Fill.substring(1);
        }

        //pull out the components
        var r = parseInt(Fill.substring(0, 2), 16);
        var g = parseInt(Fill.substring(2, 4), 16);
        var b = parseInt(Fill.substring(4, 6), 16);

        return {
            r: r / 255,
            g: g / 255,
            b: b / 255
        };
    } else if (Array.isArray(Fill)) {
        return {
            r: Fill[0],
            g: Fill[1],
            b: Fill[2]
        };
    }

    return Fill;
}

/**
 * Fill value has changed.
 */
function setFill(val, prop, obj) {
    var color = ParseRGBString(val);
    var n = amino.getCore().getNative();

    n.updateProperty(obj.handle, 'r', color.r);
    n.updateProperty(obj.handle, 'g', color.g);
    n.updateProperty(obj.handle, 'b', color.b);
}

/**
 * Set text vertical alignment.
 */
function setVAlign(val, prop, obj) {
    var n = amino.getCore().getNative();

    n.updateProperty(obj.handle, 'vAlign', n.textVAlignHash[val]);
}

/**
 * Set text wrapping mode.
 */
function setWrap(val, prop, obj) {
    var n = amino.getCore().getNative();

    n.updateProperty(obj.handle, 'wrap', n.textWrapHash[val]);
}

//native async updates
var setters = {};

['visible', 'x', 'y', 'w', 'h', 'sx', 'sy',
    'opacity', 'text', 'fontSize', 'filled',
    'rx', 'ry', 'rz', 'geometry', 'dimension', 'cliprect',
    'textureLeft', 'textureRight', 'textureTop', 'textureBottom'
].forEach(function (name) {
    setters[name] = function (val, prop, obj) {
        amino.getCore().getNative().updateProperty(obj.handle, name, val);
    };
});

setters['fill'] = setFill;
setters['vAlign'] = setVAlign;
setters['wrap'] = setWrap;

/**
 * Add native setters.
 */
function applyNativeBinding(me) {
    for (var name in setters) {
        if (me.hasOwnProperty(name)) {
            me[name].watch(setters[name]);
        }
    }
}

/**
 * Check if a given point is inside this node.
 *
 * Note: has to be used in object.
 *
 * @param pt coordinate relative to origin of node
 */
function contains(pt) {
    var x = this.x();
    var y = this.y();

    return pt.x >= 0 && pt.x < this.w() &&
           pt.y >= 0 && pt.y < this.h();
}

/**
 * Rect object.
 */
function Rect() {
    //properties
    amino.makeProps(this, {
        id: '',
        visible: true,
        x: 0,
        y: 0,
        sx: 1,
        sy: 1,

        //size
        w: 100,
        h: 100,

        //white
        fill: '#ffffff',
        opacity: 1.0
    });

    //native handle (int)
    this.handle = amino.getCore().getNative().createRect(false);

    applyNativeBinding(this);

    this.contains = contains;
    this.acceptsMouseEvents = false;
}

/**
 * Text object.
 */
function Text() {
    amino.makeProps(this, {
        id: '',
        visible: true,
        x: 0,
        y: 0,
        w: 0,
        h: 0,
        sx: 1,
        sy: 1,
        text: '',
        fontSize: 20,
        fontName: 'source',
        fontWeight: 400,
        fontStyle: 'normal',
        opacity: 1.0,
        fill: '#ffffff',
        vAlign: 'baseline',
        wrap: 'none'
    });

    //native
    this.handle = amino.getCore().getNative().createText();

    applyNativeBinding(this);

    //methods
    var self = this;

    this.updateFont = function () {
        //get font
        self.font = amino.getCore().getNative().getFont(self.fontName());

        if (!self.font) {
            self.font = amino.getCore().defaultFont;
        }

        if (self.font) {
            //get native font
            var id = self.font.getNative(self.fontSize(), self.fontWeight(), self.fontStyle());

            amino.getCore().getNative().updateProperty(self.handle, 'fontId', id);
        }
    };

    /**
     * Calc text width.
     */
    this.calcWidth = function () {
        return this.font.calcStringWidth(this.text(), this.fontSize(), this.fontWeight(), this.fontStyle());
    };

    /**
     * Calc text height.
     */
    this.calcHeight = function () {
        return this.font.getHeight(this.fontSize(), this.fontWeight(), this.fontStyle());
    };

    if (amino.getCore()) {
        this.updateFont();
    }

    this.fontName.watch(this.updateFont);
    this.fontWeight.watch(this.updateFont);
    this.fontSize.watch(this.updateFont);
}

/**
 * ImageView object.
 */
function ImageView() {
    amino.makeProps(this, {
        id: '',
        visible: true,
        x: 0,
        y: 0,
        sx: 1,
        sy: 1,

        //size
        w: 100,
        h: 100,

        //texture offset
        textureLeft:   0,
        textureRight:  1,
        textureTop:    0,
        textureBottom: 1,

        //image
        src: null,
        image: null,
        opacity: 1.0
    });

    var self = this;

    //actually load the image
    this.src.watch(function (src) {
        amino.getCore().getNative().loadImage(src, function (imageref) {
            self.image(imageref);
        });
    });

    //native
    this.handle = amino.getCore().getNative().createRect(true);

    applyNativeBinding(this);

    //when the image is loaded, update the texture id and dimensions
    this.image.watch(function (image) {
        var texid;

        if (image) {
            self.w(image.w);
            self.h(image.h);

            texid = image.texid;
        } else {
            textid = -1;
        }

        amino.getCore().getNative().updateProperty(self.handle, 'texid', texid);
    });

    this.contains = contains;
}

/**
 * Group object.
 */
function Group() {
    var core = amino.getCore();

    amino.makeProps(this, {
        id: '',
        visible:true,
        x:0,
        y:0,
        sx:1,
        sy:1,
        rx:0,
        ry:0,
        rz:0,
        w:100,
        h:100,
        cliprect:0
    });

    //native
    this.handle = core.getNative().createGroup();

    applyNativeBinding(this);

    //methods
    this.children = [];
    this.addSingle = function (node) {
        if (node == undefined) {
            throw new Error('can\'t add a null child to a group');
        }

        if (node.handle == undefined) {
            throw new Error('the child doesn\'t have a handle');
        }

        if (this.handle == undefined) {
            throw new Error('not in the scene yet');
        }

        this.children.push(node);
        node.parent = this;
        core.getNative().addNodeToGroup(node.handle, this.handle);

        return this;
    };
    this.add = function () {
        for(var i = 0; i < arguments.length; i++) {
            this.addSingle(arguments[i]);
        }

        return this;
    };
    this.remove = function (child) {
        var n = this.children.indexOf(child);

        if (n >=  0) {
            core.getNative().removeNodeFromGroup(this.children[n].handle, this.handle);
            this.children.splice(n, 1);
        }

        return this;
    };
    this.clear = function () {
        for(var i = 0; i < this.children.length; i++) {
            core.getNative().removeNodeFromGroup(this.children[i].handle, this.handle);
        }

        this.children = [];

        return this;
    };
    this.isParent = function() {
        return true;
    };

    this.raiseToTop = function (node) {
        if (node == undefined) {
            throw new Error('can\'t move a null child');
        }

        //if(node.handle == undefined) throw new Error("the child doesn't have a handle");

        this.remove(node);
        this.addSingle(node);

        return this;
    };

    function treeSearch (root, considerRoot, filter) {
        var res = [];

        if (root.isParent && root.isParent()) {
            for (var i = 0; i < root.children.length; i++) {
                res = res.concat(treeSearch(root.children[i], true, filter));
            }
        }

        if (considerRoot && filter(root)) {
            return res.concat([root]);
        }

        return res;
    }

    this.find = function (pattern) {
        var results = new FindResults();

        if (pattern.indexOf('#') == 0) {
            var id = pattern.substring(1);

            results.children = treeSearch(this, false, function (child) {
                return (child.id().toLowerCase() == id);
            });
        } else {
            results.children = treeSearch(this, false, function (child) {
                return child.constructor.name.toLowerCase() == pattern.toLowerCase();
            });
        }

        return results;
    };
}

function FindResults() {
    this.children = [];

    function makefindprop(obj, name) {
        obj[name] = function (val) {
            this.children.forEach(function (child) {
                if (child[name]) {
                    child[name](val);
                }
            });

            return this;
        };
    }

    makefindprop(this, 'visible');
    makefindprop(this, 'fill');
    makefindprop(this, 'filled');
    makefindprop(this, 'x');
    makefindprop(this, 'y');
    makefindprop(this, 'w');
    makefindprop(this, 'h');

    this.length = function () {
        return this.children.length;
    };
}

/**
 * Button object.
 */
function Button() {
    Group.call(this);

    amino.makeProps(this, {
        w: 100,
        h: 50,
        text: 'a button'
    });

    this.background = new Rect();
    this.add(this.background);
    this.background.fill("#0044cc");
    this.background.w.match(this.w);
    this.background.h.match(this.h);

    this.label = new Text().fill("#ffffff").y(20);
    this.add(this.label);
    this.label.text.match(this.text);
    this.text('a button');
    var self = this;

    this.w.watch(function() {
        var textw = self.label.calcWidth() * 2;
        var texth = self.label.calcHeight() * 2;

        self.label.x((self.w() - textw) / 2);
        self.label.y((self.h() - texth)/2 + texth);
    });

    this.contains = contains;

    var self = this;

    amino.getCore().on('press', this.background, function (e) {
        self.background.fill('#44ccff');
    });
    amino.getCore().on('release', this.background, function (e) {
        self.background.fill('#0044cc');
    });
}

/**
 * Polygon object.
 */
function Polygon() {
    amino.makeProps(this, {
        id: 'polygon',
        visible: true,
        x: 0,
        y: 0,
        sx: 1,
        sy: 1,
        closed: true,
        filled: true,
        fill: '#ff0000',
        opacity: 1.0,
        dimension: 2,
        geometry: [0,0, 50,0, 0,0]
    });

    //native
    this.handle = amino.getCore().getNative().createPoly();

    applyNativeBinding(this);

    //methods
    this.contains = function () {
        //TODO check polygon
        return false;
    };

    return this;
}

/**
 * Circle object.
 */
function Circle() {
    //get polygon properties
    Polygon.call(this);

    amino.makeProps(this, {
        radius: 50,
        steps: 30
    });

    var self = this;

    /**
     * Monitor radius updates.
     */
    this.radius.watch(function () {
        var r = self.radius();
        var points = [];
        var steps = self.steps();

        for (var i = 0; i < steps; i++) {
            var theta = Math.PI * 2 / steps * i;

            points.push(Math.sin(theta) * r);
            points.push(Math.cos(theta) * r);
        }

        self.geometry(points);
    });

    /**
     * Special case for circle.
     */
    this.contains = function (pt) {
        var radius = this.radius();
        var dist = Math.sqrt(pt.x * pt.x + pt.y * pt.y);

        return dist < radius;
    };
}

//exports
exports.Group = Group;
exports.Rect = Rect;
exports.Text = Text;
exports.Button = Button;
exports.Polygon = Polygon;
exports.Circle = Circle;
exports.ImageView = ImageView;
exports.ParseRGBString = ParseRGBString;

/**
 * PixelView object.
 */
exports.PixelView = function () {
    amino.makeProps(this, {
        id: '',
        visible: true,
        x: 0,
        y: 0,
        sx: 1,
        sy: 1,
        w: 100,
        h: 100,
        pw: 100,
        ph: 100,
        opacity: 1.0,
        fill: '#ffffff',
        textureLeft: 0,
        textureRight: 1,
        textureTop:  0,
        textureBottom: 1
    });

    var self = this;

    //native
    this.handle = amino.getCore().getNative().createRect(true);

    applyNativeBinding(this);

    //methods
    this.contains = contains;

    function rebuildBuffer() {
        var w = self.pw();
        var h = self.ph();

        self.buf = new Buffer(w * h * 4);

        var c1 = [0, 0, 0];
        var c2 = [255, 255, 255];

        for (var x = 0; x < w; x++) {
            for (var y = 0; y < h; y++) {
                var i = (x + y * w) * 4;
                var c;

                if (x % 3 == 0) {
                    c = c1;
                } else {
                    c = c2;
                }

                self.buf[i + 0] = c[0];
                self.buf[i + 1] = c[1];
                self.buf[i + 2] = c[2];
                self.buf[i + 3] = 255;
            }
        }

        self.updateTexture();
    }

    var texid = -1;

    this.updateTexture = function () {
        //when the image is loaded, update the texture id and dimensions
        var img = amino.getCore().getNative().loadBufferToTexture(texid, self.pw(), self.ph(), 4, self.buf, function (image) {
	        texid = image.texid;
            amino.getCore().getNative().updateProperty(self.handle, 'texid', image.texid);
        });
    };

    this.setPixel = function(x, y, r, g, b, a) {
        var w = self.pw();
        var i = (x + y * w)*4;

        self.buf[i + 0] = r;
        self.buf[i + 1] = g;
        self.buf[i + 2] = b;
        self.buf[i + 3] = a;
    };

    this.setPixeli32 = function(x, y, int) {
        var w = self.pw();
        var i = (x + y * w) * 4;

        self.buf.writeUInt32BE(int, i);
    };

    this.pw.watch(rebuildBuffer);
    this.ph.watch(rebuildBuffer);

    rebuildBuffer();
};

exports.PureImageView = function () {
    var piv = new exports.PixelView();
    var img = PImage.make(800, 600);
    var ctx = img.getContext('2d');

    ctx.fillStyle = '#00FF00';

    piv.getContext = function () {
        return ctx;
    };

    piv.sync = function () {
        //copy pixels
        for (var i = 0; i < img.width; i++) {
            for (var j = 0; j < img.height; j++) {
                var pixel = ctx.getPixeli32(i, j);

                if (i >= this.pw() || j >= this.ph()) {
                    continue;
                }

                this.setPixeli32(i, j, pixel);
            }
        }

        this.updateTexture();
    };

    return piv;
};

var pureimageFontsRegistered = false;

/**
 * Rich text view.
 */
exports.RichTextView = function () {
    //default view size
    var piv = new exports.PureImageView().pw(100).w(100).ph(100).h(100);

    amino.makeProps(piv, {
        multiline: true,
        enterAction: null
    });

    piv.acceptsKeyboardEvents = true;

    /**
     * Build the document.
     */
    piv.build = function (frame, done) {
        var ctx = piv.getContext();
        var config = {
            context: ctx,
            frame: frame,
            width:  piv.pw(),
            height: piv.ph(),
            multiline: piv.multiline(),
            enterAction: piv.enterAction(),
            charWidth: function (ch,
                font_size,
                font_family,
                font_weight,
                font_style
            ) {
                ctx.setFont(font_family, font_size);

                return ctx.measureText(ch).width;
            },
            requestAnimationFrame: function (redraw) {
                redraw();
                piv.sync();
            }
        };

        //register fonts
        if (!pureimageFontsRegistered) {
            pureimageFontsRegistered = true;

            var fonts = amino.getCore().getNative().getRegisteredFonts();
            var regFonts = [];

            for (var name in fonts) {
                var font = fonts[name];

                //iterate weights
                for (var weight in font.weights) {
                    var binary = font.filepaths[weight];
                    var family = font.desc.name;
                    var style = 'normal';
                    var variant = null;

                    //debug
                    //FIXME style handling
                    //console.log('font: ' + binary + ' ' + family + ' ' + style);

                    var regFont = PImage.registerFont(binary, family, weight, style, variant);

                    if (regFont) {
                        regFonts.push(regFont);
                    }
                }
            }

            //load
            async.each(regFonts, function(font, done) {
                font.load(done);
            }, function (err) {
                ready();
            });
        } else {
            ready();
        }

        //create view
        function ready() {
            var rte = comp.makeRichTextView(config);

            piv.editor = rte;
            rte.relayout();
            rte.redraw();

            amino.getCore().on('keypress', piv, function (e) {
                rte.processKeyEvent(e);
            });

            //ready
            done();
        }
    };

    return piv;
};
