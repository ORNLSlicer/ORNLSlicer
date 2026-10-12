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
