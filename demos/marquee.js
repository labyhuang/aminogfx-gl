const amino = require('../main.js')

const gfx = new amino.AminoGfx()

const opts = {
  fontSize: 96,
  colorBg: '#000000',
  colorMsg: '#00ff00',
  stepUnit: 1
}

amino.fonts.registerFont({
  name: 'default',
  path: '/home/pi/.fonts',
  weights: {
    400: {
      normal: 'NotoSansCJKtc-Regular.otf'
    }
  }
})

const msg = '世界衛生組織昨（11日）正式宣紙，武漢肺炎成全球大流行疾病（global pandemic），世衛祕書長譚德塞（Tedros Adhanom Ghebreyesus）推翻自己前日說法改口表示，預估未來不論是病例數、死亡數和受影響的國家，都會繼續攀升。'

gfx.start((core) => {
  const msgPosY = parseInt(opts.fontSize * 0.75)

  const root = gfx.createGroup()
  const rectBg = gfx.createRect().w(1920).h(opts.fontSize).x(0).y(0).fill(opts.colorBg).opacity(0.8)
  const textMsg = gfx.createText().fontName('default').fontSize(opts.fontSize).x(1920).y(msgPosY)
  const msgLen = _getStrByteLen(msg) * 0.5 * opts.fontSize
  const posEnd = 0 - msgLen - 100
  const durTime = ((msgLen + 1920) * 30) / opts.stepUnit

  gfx.opacity(0)
  gfx.setRoot(root)

  root.add(rectBg, textMsg)

  textMsg.text(msg).fill(opts.color)
  textMsg.x.anim().timeFunc('linear').from(1920).to(posEnd).dur(durTime).loop(-1).start()
})

function _getStrByteLen (str) {
  let len = 0

  for (let i = 0; i < str.length; i++) {
    let char = str.charCodeAt(i)
    while (char > 0) {
      len++
      char = char >> 8
    }
  }
  return len
}
