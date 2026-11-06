local function is_diagram_placeholder(block)
  if block.t ~= "BlockQuote" then
    return false
  end

  return pandoc.utils.stringify(block.content):match("^%s*Diagram placeholder") ~= nil
end

function BlockQuote(block)
  if not is_diagram_placeholder(block) then
    return nil
  end

  if FORMAT:match("latex") or FORMAT:match("beamer") then
    local blocks = pandoc.List()
    blocks:insert(pandoc.RawBlock("latex", "\\begin{quote}\\begingroup\\color{red}"))

    for _, child in ipairs(block.content) do
      blocks:insert(child)
    end

    blocks:insert(pandoc.RawBlock("latex", "\\par\\endgroup\\end{quote}"))
    return blocks
  end

  if FORMAT:match("html") or FORMAT:match("epub") then
    return pandoc.BlockQuote({
      pandoc.Div(
        block.content,
        pandoc.Attr("", { "diagram-placeholder" }, { style = "color: red;" })
      ),
    })
  end

  return nil
end

local function extract_html_anchor_id(text)
  return text:match('^%s*<a%s+id="([^"]+)"%s*>%s*</a>%s*$')
    or text:match('^%s*<a%s+name="([^"]+)"%s*>%s*</a>%s*$')
    or text:match('^%s*<a%s+id="([^"]+)"%s*>%s*$')
    or text:match('^%s*<a%s+name="([^"]+)"%s*>%s*$')
end

function RawBlock(el)
  if (FORMAT:match("latex") or FORMAT:match("beamer")) and el.format:match("html") then
    local id = extract_html_anchor_id(el.text)
    if id then
      return pandoc.RawBlock("latex", "\\hypertarget{" .. id .. "}{}")
    end
  end
  return nil
end

function RawInline(el)
  if (FORMAT:match("latex") or FORMAT:match("beamer")) and el.format:match("html") then
    local id = extract_html_anchor_id(el.text)
    if id then
      return pandoc.RawInline("latex", "\\hypertarget{" .. id .. "}{}")
    end
  end
  return nil
end
