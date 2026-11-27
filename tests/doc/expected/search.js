var avinDocNames = [
["units.Metres", "data", "units.html#Metres"],
["units.Metres.Metres", "variant", "units.html#Metres.Metres"],
["units.Metres.value", "field", "units.html#Metres.value"],
["units.kilometres", "function", "units.html#kilometres"],
["shapes.geometry.Point", "data", "shapes.geometry.html#Point"],
["shapes.geometry.Point.Point", "variant", "shapes.geometry.html#Point.Point"],
["shapes.geometry.Point.x", "field", "shapes.geometry.html#Point.x"],
["shapes.geometry.Point.y", "field", "shapes.geometry.html#Point.y"],
["shapes.geometry.origin", "function", "shapes.geometry.html#origin"],
["shapes.Shape", "data", "shapes.html#Shape"],
["shapes.Shape.Circle", "variant", "shapes.html#Shape.Circle"],
["shapes.Shape.Rect", "variant", "shapes.html#Shape.Rect"],
["shapes.Labelled", "data", "shapes.html#Labelled"],
["shapes.Labelled.Labelled", "variant", "shapes.html#Labelled.Labelled"],
["shapes.Labelled.label", "field", "shapes.html#Labelled.label"],
["shapes.Labelled.value", "field", "shapes.html#Labelled.value"],
["shapes.Drawing", "type", "shapes.html#Drawing"],
["shapes.Side", "type", "shapes.html#Side"],
["shapes.area", "function", "shapes.html#area"],
["shapes.area", "function", "shapes.html#area-2"],
["shapes.larger", "function", "shapes.html#larger"],
["shapes.reach", "function", "shapes.html#reach"],
["shapes.unitSquare", "value", "shapes.html#unitSquare"],
["shapes.drawn", "value", "shapes.html#drawn"],
["shapes.Named", "trait", "shapes.html#Named"],
["shapes.Named.name", "method", "shapes.html#Named.name"],
["shapes.Named.shout", "method", "shapes.html#Named.shout"],
["shapes.Canvas", "class", "shapes.html#Canvas"],
["shapes.Canvas.width", "field", "shapes.html#Canvas.width"],
["shapes.Canvas.shapes", "field", "shapes.html#Canvas.shapes"],
["shapes.Canvas.draw", "method", "shapes.html#Canvas.draw"],
["shapes.Canvas.name", "method", "shapes.html#Canvas.name"],
["shapes.Palette", "object", "shapes.html#Palette"],
["shapes.Palette.ink", "field", "shapes.html#Palette.ink"],
["shapes.geometry.Point.name", "method", "shapes.geometry.html#Point.name"],
["shapes.Shape.isEmpty", "method", "shapes.html#Shape.isEmpty"],
["shapes.Shape.name", "method", "shapes.html#Shape.name"],
];
(function () {
  var input = document.getElementById("search");
  var results = document.getElementById("results");
  if (!input || !results) return;
  input.addEventListener("input", function () {
    var q = input.value.trim().toLowerCase();
    results.textContent = "";
    if (q === "") { results.hidden = true; return; }
    var found = avinDocNames.filter(function (n) { return n[0].toLowerCase().indexOf(q) >= 0; });
    found.sort(function (a, b) {
      var ea = a[0].toLowerCase().endsWith(q) ? 0 : 1, eb = b[0].toLowerCase().endsWith(q) ? 0 : 1;
      return ea - eb || a[0].length - b[0].length || (a[0] < b[0] ? -1 : 1);
    });
    found.slice(0, 50).forEach(function (n) {
      var li = document.createElement("li");
      var a = document.createElement("a");
      a.href = n[2];
      a.textContent = n[0];
      var kind = document.createElement("span");
      kind.className = "kind";
      kind.textContent = " " + n[1];
      li.appendChild(a);
      li.appendChild(kind);
      results.appendChild(li);
    });
    results.hidden = found.length === 0;
  });
})();
